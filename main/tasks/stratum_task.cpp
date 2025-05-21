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
#include "dns_task.h"

#include "asic_jobs.h"
#include "boards/board.h"
#include "connect.h"
#include "create_jobs_task.h"
#include "global_state.h"
#include "nvs_config.h"
#include "psram_allocator.h"
#include "stratum_task.h"
#include "system.h"

#ifdef CONFIG_SPIRAM
#define ALLOC(s) heap_caps_malloc(s, MALLOC_CAP_SPIRAM)
#else
#define ALLOC(s) malloc(s)
#endif

// fallback can nicely be tested with netcat
// mkfifo /tmp/ncpipe
// nc -l -p 4444 < /tmp/ncpipe | nc solo.ckpool.org 3333 > /tmp/ncpipe

enum Selected
{
    PRIMARY = 0,
    SECONDARY = 1
};

static void safe_free(char *&ptr)
{
    if (ptr) {         // Check if pointer is not null
        free(ptr);     // Free memory
        ptr = nullptr; // Set pointer to null to prevent dangling pointer issues
    }
}

int is_socket_connected(int socket)
{
    if (socket == -1) {
        return 0;
    }
    struct timeval tv;
    fd_set writefds;

    tv.tv_sec = 0;
    tv.tv_usec = 100000; // 100 ms timeout

    FD_ZERO(&writefds);
    FD_SET(socket, &writefds);

    int ret = select(socket + 1, NULL, &writefds, NULL, &tv);
    return (ret > 0 && FD_ISSET(socket, &writefds)) ? 1 : 0;
}

StratumTask::StratumTask(StratumManager *manager, int index, StratumConfig *config)
{
    m_manager = manager;
    m_config = config;
    m_index = index;

    if (config->primary) {
        m_tag = "stratum task";
    } else {
        m_tag = "stratum task (fallback)";
    }
}

bool StratumTask::isWifiConnected()
{
    wifi_ap_record_t ap_info;
    return esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK;
}

int StratumTask::connectStratum(const char *host_ip, uint16_t port)
{
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(host_ip);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);

    int addr_family = AF_INET;
    int ip_protocol = IPPROTO_IP;

    int sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (sock < 0) {
        return 0;
    }
    ESP_LOGI(m_tag, "Socket created, connecting to %s:%d", host_ip, port);

    int err = ::connect(sock, (struct sockaddr *) &dest_addr, sizeof(struct sockaddr_in6));
    if (err != 0) {
        shutdown(sock, SHUT_RDWR);
        close(sock);
        return 0;
    }

    ESP_LOGI(m_tag, "Connected");

    if (!setupSocketTimeouts(sock)) {
        ESP_LOGE(m_tag, "Error setting socket timeouts");
    }

    return sock;
}

bool StratumTask::setupSocketTimeouts(int sock)
{
    // we add timeout to prevent recv to hang forever
    // if it times out on the recv we will check the connection state
    // and retry if still connected
    struct timeval timeout;
    timeout.tv_sec = 30; // 30 seconds timeout
    timeout.tv_usec = 0; // 0 microseconds

    ESP_LOGI(m_tag, "Set socket timeout to %d for recv and write", (int) timeout.tv_sec);
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        ESP_LOGE(m_tag, "Failed to set socket receive timeout");
        return false;
    }

    if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        ESP_LOGE(m_tag, "Failed to set socket send timeout");
        return false;
    }
    return true;
}

void StratumTask::taskWrapper(void *pvParameters)
{
    StratumTask *task = (StratumTask *) pvParameters;
    task->task();
}

void StratumTask::task()
{
    while (1) {
        // do we have a stratum host configured?
        // we do it here because we could reload the config after
        // it was updated on the UI and settings
        if (!strlen(m_config->host)) {
            vTaskDelay(10000 / portTICK_PERIOD_MS);
            continue;
        }

        // should stay stopped?
        if (m_stopFlag) {
            vTaskDelay(10000 / portTICK_PERIOD_MS);
            continue;
        }

        // check if wifi is connected
        // esp_wifi_connect is thread safe
        if (!isWifiConnected()) {
            ESP_LOGI(m_tag, "WiFi disconnected, attempting to reconnect...");
            esp_wifi_connect();
            vTaskDelay(10000 / portTICK_PERIOD_MS);
            continue;
        }

        // resolve the IP of the host
        char ip[INET_ADDRSTRLEN] = {0};
        ip_addr_t target_addr;
        if (!resolve_hostname(m_config->host, &target_addr)) {
            ESP_LOGE(m_tag, "%s couldn't be resolved!", m_config->host);
            vTaskDelay(10000 / portTICK_PERIOD_MS);
            continue;
        }
        strncpy(ip, ipaddr_ntoa(&target_addr), sizeof(ip) - 1);
        ip[sizeof(ip) - 1] = '\0';  // Ensure null termination

        ESP_LOGI(m_tag, "Connecting to: stratum+tcp://%s:%d (%s)", m_config->host, m_config->port, ip);

        if (!(m_sock = connectStratum(ip, m_config->port))) {
            ESP_LOGE(m_tag, "Socket unable to connect to %s:%d (errno %d)", m_config->host, m_config->port, errno);
            vTaskDelay(10000 / portTICK_PERIOD_MS);
            continue;
        }

        // we are connected but it doesn't mean the server is alive ...

        // stratum loop
        stratumLoop();

        // track pool errors
        // TODO: move this into the manager and mutex it
        SYSTEM_MODULE.incPoolErrors();

        // shutdown and reconnect
        ESP_LOGE(m_tag, "Shutdown socket ...");
        shutdown(m_sock, SHUT_RDWR);
        close(m_sock);

        // mark invalid
        m_sock = -1;

        m_manager->disconnectedCallback(m_index);
        m_isConnected = false;

        vTaskDelay(10000 / portTICK_PERIOD_MS); // Delay before attempting to reconnect
    }
    vTaskDelete(NULL);
}

void StratumTask::stratumLoop()
{
    Board *board = SYSTEM_MODULE.getBoard();

    m_stratumAPI.resetUid();
    m_stratumAPI.clearBuffer();

    ///// Start Stratum Action
    // mining.subscribe - ID: 1
    bool success = m_stratumAPI.subscribe(m_sock, board->getMiningAgent(), board->getAsicModel());

    // mining.configure - ID: 2
    success = success && m_stratumAPI.configureVersionRolling(m_sock);

    // mining.authorize - ID: 3
    success = success && m_stratumAPI.authenticate(m_sock, m_config->user, m_config->password);

    // mining.suggest_difficulty - ID: 4
    success = success && m_stratumAPI.suggestDifficulty(m_sock, Config::getStratumDifficulty());

    if (!success) {
        ESP_LOGE(m_tag, "Error sending Stratum setup commands!");
        return;
    }

    // All Stratum servers should send the first job with clear flag,
    // but we make sure to clear the jobs on the first job
    m_firstJob = true;

    char *line = nullptr;

    while (1) {
        if (!is_socket_connected(m_sock)) {
            ESP_LOGE(m_tag, "Socket is not connected ...");
            break;
        }

        line = m_stratumAPI.receiveJsonRpcLine(m_sock);
        if (!line) {
            ESP_LOGE(m_tag, "Failed to receive JSON-RPC line, reconnecting ...");
            break;
        }

        ESP_LOGI(m_tag, "rx: %s", line); // debug incoming stratum messages

        PSRAMAllocator allocator;
        JsonDocument doc(&allocator);

        // Deserialize JSON
        // we want to know if it's valid json before the connected callback is executed
        DeserializationError error = deserializeJson(doc, line);
        if (error) {
            ESP_LOGE(m_tag, "Unable to parse JSON: %s", error.c_str());
            break;
        }

        // we are pretty confident now that we have valid json and we can
        // call the connected callback
        if (!m_isConnected) {
            m_manager->connectedCallback(m_index);
            m_isConnected = true;
        }

        // if stop is requested, don't dispatch anything
        // and break the loop
        if (m_stopFlag) {
            break;
        }

        // parse the line
        m_manager->dispatch(m_index, doc);

        // sets line to nullptr too
        safe_free(line);
    }

    safe_free(line);
}

void StratumTask::submitShare(const char *jobid, const char *extranonce_2, const uint32_t ntime, const uint32_t nonce,
                              const uint32_t version)
{
    m_stratumAPI.submitShare(m_sock, m_config->user, jobid, extranonce_2, ntime, nonce, version);
}

void StratumTask::connect()
{
    m_stopFlag = false;
}

void StratumTask::disconnect()
{
    m_stopFlag = true;
}

StratumManager::StratumManager()
{
    m_stratum_api_v1_message = (StratumApiV1Message *) ALLOC(sizeof(StratumApiV1Message));
}

bool StratumManager::isUsingFallback()
{
    return m_selected != Selected::PRIMARY;
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

bool StratumManager::isAnyConnected() {
    return isConnected(PRIMARY) || isConnected(SECONDARY);
}

void StratumManager::reconnectTimerCallbackWrapper(TimerHandle_t xTimer)
{
    StratumManager *self = static_cast<StratumManager *>(pvTimerGetTimerID(xTimer));
    self->reconnectTimerCallback(xTimer);
}

// Reconnect Timer Callback
void StratumManager::reconnectTimerCallback(TimerHandle_t xTimer)
{
    pthread_mutex_lock(&m_mutex);
    // Check if primary is still disconnected
    if (!isConnected(Selected::PRIMARY)) {
        connect(Selected::SECONDARY);
        m_selected = Selected::SECONDARY;
    }
    pthread_mutex_unlock(&m_mutex);
}

// Start the reconnect timer
void StratumManager::startReconnectTimer()
{
    if (m_reconnectTimer == NULL) {
        m_reconnectTimer = xTimerCreate("Reconnect Timer", pdMS_TO_TICKS(30000), pdTRUE, this, reconnectTimerCallbackWrapper);
    }

    // only start the timer when it is not running
    if (xTimerIsTimerActive(m_reconnectTimer) == pdFALSE) {
        xTimerStart(m_reconnectTimer, 0);
    }
}

// Stop the reconnect timer
void StratumManager::stopReconnectTimer()
{
    if (m_reconnectTimer != NULL) {
        xTimerStop(m_reconnectTimer, 0);
    }
}

// Connected Callback
void StratumManager::connectedCallback(int index)
{
    pthread_mutex_lock(&m_mutex);

    if (index == Selected::PRIMARY) {
        m_selected = Selected::PRIMARY;
        disconnect(Selected::SECONDARY);
        stopReconnectTimer(); // Stop reconnect attempts
    }

    pthread_mutex_unlock(&m_mutex);
}

// Disconnected Callback
void StratumManager::disconnectedCallback(int index)
{
    startReconnectTimer(); // Start the timer to attempt reconnects
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

    // Create the Stratum tasks for both pools
    for (int i = 0; i < 2; i++) {
        m_stratumTasks[i] = new StratumTask(this, i, system->getStratumConfig(i));
        xTaskCreate(m_stratumTasks[i]->taskWrapper, (i == 0 ? "stratum task (primary)" : "stratum task (secondary)"), 8192,
                    (void *) m_stratumTasks[i], 5, NULL);
    }

    // Always start by connecting to the primary pool
    connect(Selected::PRIMARY);

    // Start the reconnect timer
    startReconnectTimer();

    // Watchdog Task Loop (optional, if needed)
    while (1) {
        vTaskDelay(30000 / portTICK_PERIOD_MS);

        // Reset watchdog if there was a submit response within the last hour
        if (((esp_timer_get_time() - m_lastSubmitResponseTimestamp) / 1000000) < 3600) {
            esp_task_wdt_reset();
        }
    }
}

void StratumManager::cleanQueue()
{
    ESP_LOGI(m_tag, "Clean Jobs: clearing queue");
    asicJobs.cleanJobs();
}

const char *StratumManager::getCurrentPoolHost()
{
    if (!m_stratumTasks[m_selected]) {
        return "-";
    }
    return m_stratumTasks[m_selected]->getHost();
}

int StratumManager::getCurrentPoolPort()
{
    if (!m_stratumTasks[m_selected]) {
        return 0;
    }
    return m_stratumTasks[m_selected]->getPort();
}

void StratumManager::dispatch(int pool, JsonDocument &doc)
{
    // only accept data from the selected pool
    if (pool != m_selected) {
        return;
    }

    StratumTask *selected = m_stratumTasks[m_selected];

    const char *tag = selected->getTag();

    memset(m_stratum_api_v1_message, 0, sizeof(m_stratum_api_v1_message));

    if (!StratumApi::parse(m_stratum_api_v1_message, doc)) {
        ESP_LOGE(m_tag, "error in stratum");
        return;
    }

    switch (m_stratum_api_v1_message->method) {
    case MINING_NOTIFY: {
        SYSTEM_MODULE.notifyNewNtime(m_stratum_api_v1_message->mining_notification->ntime);

        // abandon work clears the asic job list
        // also clear on first job
        if (selected->m_firstJob || m_stratum_api_v1_message->should_abandon_work) {
            cleanQueue();
            selected->m_firstJob = false;
        }
        create_job_mining_notify(m_stratum_api_v1_message->mining_notification);

        // free notify
        if (m_stratum_api_v1_message->mining_notification) {
            StratumApi::freeMiningNotify(m_stratum_api_v1_message->mining_notification);
            free(m_stratum_api_v1_message->mining_notification);
        }
        break;
    }

    case MINING_SET_DIFFICULTY: {
        SYSTEM_MODULE.setPoolDifficulty(m_stratum_api_v1_message->new_difficulty);
        if (create_job_set_difficulty(m_stratum_api_v1_message->new_difficulty)) {
            ESP_LOGI(tag, "Set stratum difficulty: %ld", m_stratum_api_v1_message->new_difficulty);
        }
        break;
    }

    case MINING_SET_VERSION_MASK:
    case STRATUM_RESULT_VERSION_MASK: {
        ESP_LOGI(tag, "Set version mask: %08lx", m_stratum_api_v1_message->version_mask);
        create_job_set_version_mask(m_stratum_api_v1_message->version_mask);
        break;
    }

    case STRATUM_RESULT_SUBSCRIBE: {
        ESP_LOGI(tag, "Set enonce %s enonce2-len: %d", m_stratum_api_v1_message->extranonce_str,
                 m_stratum_api_v1_message->extranonce_2_len);
        create_job_set_enonce(m_stratum_api_v1_message->extranonce_str, m_stratum_api_v1_message->extranonce_2_len);
        break;
    }

    case CLIENT_RECONNECT: {
        ESP_LOGE(tag, "Pool requested client reconnect ...");
        break;
    }

    case STRATUM_RESULT: {
        if (m_stratum_api_v1_message->response_success) {
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
        if (m_stratum_api_v1_message->response_success) {
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
}

void StratumManager::submitShare(const char *jobid, const char *extranonce_2, const uint32_t ntime, const uint32_t nonce,
                                 const uint32_t version)
{
    // send to the selected pool
    if (!m_stratumTasks[m_selected]->m_isConnected) {
        ESP_LOGE(m_tag, "selected pool not connected");
        return;
    }
    m_stratumTasks[m_selected]->submitShare(jobid, extranonce_2, ntime, nonce, version);
}
