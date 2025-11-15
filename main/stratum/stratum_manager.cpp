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

#include "stratum_manager.h"

// ------------  stratum manager
StratumManager::StratumManager(PoolMode poolmode) : m_poolmode(poolmode), m_lastSubmitResponseTimestamp(0)
{
    m_stratumTasks[0] = nullptr;
    m_stratumTasks[1] = nullptr;
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

int StratumManager::getNumConnectedPools() {
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
        m_stratumTasks[i] = new StratumTask(this, i, system->getStratumConfig(i));
        xTaskCreate(m_stratumTasks[i]->taskWrapper, (i == 0 ? "stratum task (primary)" : "stratum task (secondary)"), 8192,
                    (void *) m_stratumTasks[i], 5, NULL);
    }

    if (m_poolmode == PoolMode::DUAL) {
        connect(PRIMARY);
        connect(SECONDARY);
    } else {
        connect(PRIMARY);
    }

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

void StratumManager::cleanQueue()
{
    ESP_LOGI(m_tag, "Clean Jobs: clearing queue");
    asicJobs.cleanJobs();
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
        SYSTEM_MODULE.notifyNewNtime(m_stratum_api_v1_message.mining_notification->ntime);

        // abandon work clears the asic job list
        // also clear on first job
        if (selected->m_firstJob || m_stratum_api_v1_message.should_abandon_work) {
            cleanQueue();
            selected->m_firstJob = false;
        }
        create_job_mining_notify(pool, m_stratum_api_v1_message.mining_notification);
        if (m_stratum_api_v1_message.mining_notification->ntime) {
            m_stratumTasks[pool]->m_validNotify = true;
        }
        break;
    }

    case MINING_SET_DIFFICULTY: {
        SYSTEM_MODULE.setPoolDifficulty(m_stratum_api_v1_message.new_difficulty);
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
            SYSTEM_MODULE.notifyAcceptedShare();
        } else {
            ESP_LOGW(tag, "message result rejected");
            SYSTEM_MODULE.notifyRejectedShare();
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
    // send to the selected pool
    if (!m_stratumTasks[pool]->m_isConnected) {
        ESP_LOGE(m_tag, "selected pool not connected");
        return;
    }
    m_stratumTasks[pool]->submitShare(jobid, extranonce_2, ntime, nonce, version);
}
