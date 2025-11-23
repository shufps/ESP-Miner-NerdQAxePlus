#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>

#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "global_state.h"
#include "lwip/dns.h"

#include "asic_jobs.h"
#include "boards/board.h"
#include "connect.h"
#include "create_jobs_task.h"
#include "global_state.h"
#include "macros.h"
#include "nvs_config.h"
#include "psram_allocator.h"
#include "stratum_task.h"
#include "system.h"

#include "stratum_config.h"
#include "stratum_manager.h"
#include "utils.h"

// ------------  stratum manager
StratumManager::StratumManager(PoolMode poolmode) : m_poolmode(poolmode), m_lastSubmitResponseTimestamp(0)
{
    m_stratumTasks[0] = nullptr;
    m_stratumTasks[1] = nullptr;

    suffixString(0, m_totalBestDiffString, DIFF_STRING_SIZE, 0);
    suffixString(0, m_bestSessionDiffString, DIFF_STRING_SIZE, 0);
}

void StratumManager::connect(int index)
{
    m_stratumTasks[index]->connect();
}

void StratumManager::disconnect(int index)
{
    m_stratumTasks[index]->disconnect();
}

bool StratumManager::isConnected(int index)
{
    return m_stratumTasks[index] && m_stratumTasks[index]->isConnected();
}

bool StratumManager::isAnyConnected()
{
    return isConnected(PRIMARY) || isConnected(SECONDARY);
}

int StratumManager::getNumConnectedPools()
{
    return !!isConnected(PRIMARY) + !!isConnected(SECONDARY);
}

// This static wrapper converts the void* parameter into a StratumManager pointer
// and calls the member function.
void StratumManager::taskWrapper(void *pvParameters)
{
    StratumManager *manager = static_cast<StratumManager *>(pvParameters);
    manager->task();
}

void StratumManager::task()
{
    System *system = &SYSTEM_MODULE;

    ESP_LOGI("StratumManager", "Subscribing to task watchdog.");
    if (esp_task_wdt_add(NULL) != ESP_OK) {
        ESP_LOGE("StratumManager", "Failed to add task to watchdog!");
    }

    ESP_LOGI("StratumManager", "%s mode enabled", m_poolmode == PoolMode::DUAL ? "Dual Pool" : "Failover");

    // Create the Stratum tasks for both pools
    for (int i = 0; i < 2; i++) {
        m_stratumTasks[i] = new StratumTask(this, i);
        xTaskCreate(m_stratumTasks[i]->taskWrapper, (i == 0) ? "stratum task (pri)" : "stratum task (sec)", 8192,
                    (void *) m_stratumTasks[i], 5, NULL);

        m_pingTasks[i] = new PingTask(this, i);
        xTaskCreate(m_pingTasks[i]->ping_task_wrapper, (i == 0) ? "ping task (pri)" : "ping task (sec)", 4096,
                    (void *) m_pingTasks[i], 1, NULL);
    }

    if (m_poolmode == PoolMode::DUAL) {
        connect(PRIMARY);
        connect(SECONDARY);
    } else {
        connect(PRIMARY);
    }

    m_initialized = true;

    // Watchdog Task Loop (optional, if needed)
    while (1) {
        if (POWER_MANAGEMENT_MODULE.isShutdown()) {
            // remove watchdog
            esp_err_t err = esp_task_wdt_delete(NULL);
            if (err != ESP_OK) {
                ESP_LOGE(m_tag, "Couldn't remove watchdog");
            }
            ESP_LOGW(m_tag, "suspended");
            vTaskSuspend(NULL);
        }
        vTaskDelay(pdMS_TO_TICKS(30000));

        // Reset watchdog if there was a submit response within the last hour
        if (m_lastSubmitResponseTimestamp && ((esp_timer_get_time() - m_lastSubmitResponseTimestamp) / 1000000) < 3600) {
            esp_task_wdt_reset();
        }
    }
}

void StratumManager::freeStratumV1Message(StratumApiV1Message *message)
{
    if (!message) {
        return;
    }
    StratumApi::freeMiningNotify(message->mining_notification);
    safe_free(message->mining_notification);
    safe_free(message->extranonce_str);
}

void StratumManager::dispatch(int pool, JsonDocument &doc)
{
    // ensure consistent use of m_stratum_api_v1_message
    PThreadGuard lock(m_mutex);

    if (!acceptsNotifyFrom(pool)) {
        return;
    }

    StratumTask *selected = m_stratumTasks[pool];

    if (!selected) {
        ESP_LOGE(m_tag, "stratum task is null");
        return;
    }

    const char *tag = selected->getTag();

    // we keep the m_stratum_api_v1_message in the class
    // to not have the struct on the stack
    memset(&m_stratum_api_v1_message, 0, sizeof(StratumApiV1Message));

    if (!StratumApi::parse(&m_stratum_api_v1_message, doc)) {
        ESP_LOGE(m_tag, "error in stratum");
        // free memory
        freeStratumV1Message(&m_stratum_api_v1_message);
        return;
    }

    switch (m_stratum_api_v1_message.method) {
    case MINING_NOTIFY: {
        create_job_mining_notify(pool, m_stratum_api_v1_message.mining_notification,
                                 m_stratum_api_v1_message.should_abandon_work || selected->m_firstJob);

        if (m_stratum_api_v1_message.mining_notification->ntime) {
            m_stratumTasks[pool]->m_validNotify = true;
        }

        selected->m_firstJob = false;
        break;
    }

    case MINING_SET_DIFFICULTY: {
        setPoolDifficulty(pool, m_stratum_api_v1_message.new_difficulty);
        if (create_job_set_difficulty(pool, m_stratum_api_v1_message.new_difficulty)) {
            ESP_LOGI(tag, "Set stratum difficulty: %ld", m_stratum_api_v1_message.new_difficulty);
        }
        break;
    }

    case MINING_SET_VERSION_MASK:
    case STRATUM_RESULT_VERSION_MASK: {
        ESP_LOGI(tag, "Set version mask: %08lx", m_stratum_api_v1_message.version_mask);
        create_job_set_version_mask(pool, m_stratum_api_v1_message.version_mask);
        break;
    }

    case MINING_SET_EXTRANONCE: {
        // the new extranonce gets active with the next mining.notify
        ESP_LOGI(tag, "Set next enonce %s enonce2-len: %d", m_stratum_api_v1_message.extranonce_str,
                 m_stratum_api_v1_message.extranonce_2_len);
        set_next_enonce(pool, m_stratum_api_v1_message.extranonce_str, m_stratum_api_v1_message.extranonce_2_len);
        break;
    }

    case STRATUM_RESULT_SUBSCRIBE: {
        ESP_LOGI(tag, "Set enonce %s enonce2-len: %d", m_stratum_api_v1_message.extranonce_str,
                 m_stratum_api_v1_message.extranonce_2_len);
        create_job_set_enonce(pool, m_stratum_api_v1_message.extranonce_str, m_stratum_api_v1_message.extranonce_2_len);
        break;
    }

    case CLIENT_RECONNECT: {
        ESP_LOGE(tag, "Pool requested client reconnect ...");
        break;
    }

    case STRATUM_RESULT: {
        if (m_stratum_api_v1_message.response_success) {
            ESP_LOGI(tag, "message result accepted");
            acceptedShare(pool);
        } else {
            ESP_LOGW(tag, "message result rejected");
            rejectedShare(pool);
        }
        m_lastSubmitResponseTimestamp = esp_timer_get_time();
        break;
    }

    case STRATUM_RESULT_SETUP: {
        if (m_stratum_api_v1_message.response_success) {
            ESP_LOGI(tag, "setup message accepted");
        } else {
            ESP_LOGE(tag, "setup message rejected");
        }
        break;
    }

    default: {
        // NOP
    }
    }

    // free memory
    freeStratumV1Message(&m_stratum_api_v1_message);
}

void StratumManager::submitShare(int pool, const char *jobid, const char *extranonce_2, const uint32_t ntime, const uint32_t nonce,
                                 const uint32_t version)
{
    if (!m_stratumTasks[pool]) {
        ESP_LOGE(m_tag, "stratum task is null");
        return;
    }
    // send to the selected pool
    if (!m_stratumTasks[pool]->m_isConnected) {
        ESP_LOGE(m_tag, "selected pool not connected");
        return;
    }
    m_stratumTasks[pool]->submitShare(jobid, extranonce_2, ntime, nonce, version);
}

// --- stratum config related; mutexed
const StratumConfig StratumManager::getStratumConfig(int i) {
    PThreadGuard lock(m_mutex);

    // returns a copy via copy constructor
    return m_stratumConfig[i];
}

void StratumManager::loadSettings(bool reconnect)
{
    m_totalBestDiff = Config::getBestDiff();
    m_totalFoundBlocks = Config::getTotalFoundBlocks();

    suffixString(m_totalBestDiff, m_totalBestDiffString, DIFF_STRING_SIZE, 0);

    // init with the reconnect flag from the child class
    bool requiresReconnect[2] = {reconnect, reconnect};

    // load and compare config
    for (int i=0; i<2; i++) {
        StratumConfig tmp = StratumConfig::read(i);
        if (!m_stratumConfig[i].isEqual(tmp)) {
            requiresReconnect[i] = true;
        }
        m_stratumConfig[i] = tmp; // deep copy!

    }

    // reconnect the pools with changed configs
    for (int i=0;i<2;i++) {
        if (!requiresReconnect[i]) {
            continue;
        }
        // trigger a reconnect
        if (m_stratumTasks[i]) {
            m_stratumTasks[i]->triggerReconnect();
        }

        // reset ping stats
        if (m_pingTasks[i]) {
            m_pingTasks[i]->reset();
        }
    }
}

void StratumManager::saveSettings(const JsonDocument &doc) {
    if (doc["poolMode"].is<uint16_t>()) {
        Config::setPoolMode(doc["poolMode"].as<uint16_t>());
    }
    if (doc["stratumURL"].is<const char*>()) {
        Config::setStratumURL(doc["stratumURL"].as<const char*>());
    }
    if (doc["stratumUser"].is<const char*>()) {
        Config::setStratumUser(doc["stratumUser"].as<const char*>());
    }
    if (doc["stratumPassword"].is<const char*>()) {
        Config::setStratumPass(doc["stratumPassword"].as<const char*>());
    }
    if (doc["stratumPort"].is<uint16_t>()) {
        Config::setStratumPortNumber(doc["stratumPort"].as<uint16_t>());
    }
    if (doc["stratumEnonceSubscribe"].is<bool>()) {
        Config::setStratumEnonceSubscribe(doc["stratumEnonceSubscribe"].as<bool>());
    }
    if (doc["fallbackStratumURL"].is<const char*>()) {
        Config::setStratumFallbackURL(doc["fallbackStratumURL"].as<const char*>());
    }
    if (doc["fallbackStratumUser"].is<const char*>()) {
        Config::setStratumFallbackUser(doc["fallbackStratumUser"].as<const char*>());
    }
    if (doc["fallbackStratumPassword"].is<const char*>()) {
        Config::setStratumFallbackPass(doc["fallbackStratumPassword"].as<const char*>());
    }
    if (doc["fallbackStratumPort"].is<uint16_t>()) {
        Config::setStratumFallbackPortNumber(doc["fallbackStratumPort"].as<uint16_t>());
    }
    if (doc["fallbackStratumEnonceSubscribe"].is<bool>()) {
        Config::setStratumFallbackEnonceSubscribe(doc["fallbackStratumEnonceSubscribe"].as<bool>());
    }
}

// ---

void StratumManager::checkForBestDiff(int pool, double diff, uint32_t nbits)
{
    if ((uint64_t) diff <= m_totalBestDiff) {
        return;
    }
    m_totalBestDiff = (uint64_t) diff;
    suffixString((uint64_t) diff, m_totalBestDiffString, DIFF_STRING_SIZE, 0);

    Config::setBestDiff(m_totalBestDiff);
}

void StratumManager::getManagerInfoJson(JsonObject &obj)
{
    obj["poolMode"] = Config::getPoolMode();
    obj["activePoolMode"] = getPoolMode();

    obj["totalBestDiff"] = m_totalBestDiff;
}

void StratumManager::checkForFoundBlock(int pool, double diff, uint32_t nbits)
{
    double networkDiff = calculateNetworkDifficulty(nbits);
    if (diff <= networkDiff) {
        return;
    }

    ESP_LOGI(m_tag, "FOUND BLOCK!!! %f > %f", diff, networkDiff);

    // increase total found blocks counter
    m_foundBlocks++;
    m_totalFoundBlocks++;
    Config::setTotalFoundBlocks(m_totalFoundBlocks);

    discordAlerter.sendBlockFoundAlert(diff, networkDiff);
}

const char *StratumManager::getResolvedIpForPool(int pool) const
{
    if (!m_stratumTasks[pool]) {
        return nullptr;
    }
    return m_stratumTasks[pool]->getResolvedIp();
}



double get_last_ping_rtt()
{
    if (!STRATUM_MANAGER) {
        return 0.0;
    }
    int idx = STRATUM_MANAGER->getCompatPingPoolIndex();
    PingTask *task = STRATUM_MANAGER->getPingTask(idx); // kleiner Getter
    return task ? task->get_last_ping_rtt() : 0.0;
}

double get_recent_ping_loss()
{
    if (!STRATUM_MANAGER) {
        return 0.0;
    }
    int idx = STRATUM_MANAGER->getCompatPingPoolIndex();
    PingTask *task = STRATUM_MANAGER->getPingTask(idx);
    return task ? task->get_recent_ping_loss() : 0.0;
}

