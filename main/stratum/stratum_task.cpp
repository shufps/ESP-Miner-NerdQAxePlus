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

// fallback can nicely be tested with netcat
// mkfifo /tmp/ncpipe
// nc -l -p 4444 < /tmp/ncpipe | nc solo.ckpool.org 3333 > /tmp/ncpipe

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

StratumTask::StratumTask(StratumManager *manager, int index)
    : m_manager(manager), m_index(index), m_sock(-1), m_isConnected(false), m_stopFlag(false), m_firstJob(true),
      m_validNotify(false), m_reloadConfig(false), m_reconnectTimer(nullptr)
{
    m_primary = !index;

    if (m_primary) {
        m_tag = "stratum task (Pri)";
    } else {
        m_tag = "stratum task (Sec)";
    }
}

bool StratumTask::isWifiConnected()
{
    wifi_ap_record_t ap_info;
    return esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK;
}

bool StratumTask::resolveHostname(const char *hostname, char *ip_str, size_t ip_str_len)
{
    struct addrinfo hints;
    struct addrinfo *res;
    int err;

    // Set up the hints for the lookup
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // IPv4

    // Perform the blocking DNS lookup
    err = getaddrinfo(hostname, NULL, &hints, &res);
    if (err != 0 || res == NULL) {
        ESP_LOGE("DNS", "DNS lookup failed: %d", err);
        return false;
    }

    // Extract the IP address from the result
    struct sockaddr_in *addr = (struct sockaddr_in *) res->ai_addr;
    inet_ntop(AF_INET, &(addr->sin_addr), ip_str, ip_str_len);

    // save IP adrdress
    strncpy(m_lastResolvedIp, ip_str, sizeof(m_lastResolvedIp));
    m_lastResolvedIp[sizeof(m_lastResolvedIp) - 1] = '\0';

    ESP_LOGI("DNS", "Resolved IP: %s", ip_str);

    // Free the result
    freeaddrinfo(res);

    return true;
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

    int err = ::connect(sock, (struct sockaddr *) &dest_addr, sizeof(dest_addr));
    if (err != 0) {
        ESP_LOGE(m_tag, "Connect failed to %s:%d (errno %d: %s)", host_ip, port, errno, strerror(errno));
        shutdown(sock, SHUT_RDWR);
        close(sock);
        return 0;
    }

    ESP_LOGI(m_tag, "Connected to %s:%d", host_ip, port);

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

    // Enable TCP Keepalive
    int enable = Config::isStratumKeepaliveEnabled() ? 1 : 0;
    if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &enable, sizeof(enable)) < 0) {
        ESP_LOGE(m_tag, "Failed to enable SO_KEEPALIVE");
        return false;
    }

    if (enable) {
        // Configure Keepalive parameters
        int keepidle = 10; // Start sending keepalive probes after 10 seconds of inactivity
        int keepintvl = 5; // Interval of 5 seconds between individual keepalive probes
        int keepcnt = 3;   // Disconnect after 3 unanswered probes

        if (setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(keepidle)) < 0) {
            ESP_LOGW(m_tag, "TCP_KEEPIDLE not supported or failed to set");
            // This might not be critical, so we could just log a warning and continue
        }
        if (setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(keepintvl)) < 0) {
            ESP_LOGE(m_tag, "Failed to set TCP_KEEPINTVL");
        }
        if (setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(keepcnt)) < 0) {
            ESP_LOGE(m_tag, "Failed to set TCP_KEEPCNT");
        }

        ESP_LOGI(m_tag, "TCP Keepalive enabled: idle=%ds, interval=%ds, count=%d", keepidle, keepintvl, keepcnt);
    } else {
        ESP_LOGI(m_tag, "TCP Keepalive is disabled via config.");
    }

    return true;
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
    success = success && m_stratumAPI.authenticate(m_sock, m_config.user, m_config.password);

    // mining.suggest_difficulty - ID: 4
    success = success && m_stratumAPI.suggestDifficulty(m_sock, Config::getStratumDifficulty());

    // mining.mining.extranonce.subscribe - ID 5
    if (m_config.enonceSub) {
        success = success && m_stratumAPI.entranonceSubscribe(m_sock);
    }

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
            if (Config::isStratumKeepaliveEnabled()) {
                ESP_LOGW(m_tag, "Socket disconnected — possible TCP KeepAlive timeout (enabled)");
            } else {
                ESP_LOGW(m_tag, "Socket disconnected — no KeepAlive active");
            }
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
            connectedCallback();
            m_isConnected = true;
        }

        if (m_reloadConfig) {
            ESP_LOGI(m_tag, "stratum config reload requested.");
            break;
        }
        // if stop is requested, don't dispatch anything
        // and break the loop
        if (m_stopFlag || POWER_MANAGEMENT_MODULE.isShutdown()) {
            break;
        }

        // parse the line
        m_manager->dispatch(m_index, doc);

        // sets line to nullptr too
        safe_free(line);
    }

    safe_free(line);
}

void StratumTask::connect()
{
    m_stopFlag = false;
}

void StratumTask::disconnect()
{
    m_stopFlag = true;
}

void StratumTask::reconnectTimerCallbackWrapper(TimerHandle_t xTimer)
{
    StratumTask *self = static_cast<StratumTask *>(pvTimerGetTimerID(xTimer));
    self->reconnectTimerCallback(xTimer);
}

// Reconnect Timer Callback
void StratumTask::reconnectTimerCallback(TimerHandle_t xTimer)
{
    m_manager->reconnectTimerCallback(m_index);
}

// Connected Callback
void StratumTask::connectedCallback()
{
    m_manager->connectedCallback(m_index);
}

// Disconnected Callback
void StratumTask::disconnectedCallback()
{
    m_manager->disconnectedCallback(m_index);
}

// Start the reconnect timer
void StratumTask::startReconnectTimer()
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
void StratumTask::stopReconnectTimer()
{
    if (m_reconnectTimer != NULL) {
        xTimerStop(m_reconnectTimer, 0);
    }
}

void StratumTask::submitShare(const char *jobid, const char *extranonce_2, const uint32_t ntime, const uint32_t nonce,
                              const uint32_t version)
{
    m_stratumAPI.submitShare(m_sock, m_config.user, jobid, extranonce_2, ntime, nonce, version);
}

void StratumTask::taskWrapper(void *pvParameters)
{
    StratumTask *task = (StratumTask *) pvParameters;
    task->task();
}

void StratumTask::loadConfig() {
    // release previous config
    StratumConfigReader::releaseConfig(m_config);

    if (m_primary) {
        m_config = StratumConfigReader::readPrimary();
    } else {
        m_config = StratumConfigReader::readFallback();
    }
}

void StratumTask::task()
{
    loadConfig();

    // Start the reconnect timer
    startReconnectTimer();

    while (1) {
        if (POWER_MANAGEMENT_MODULE.isShutdown()) {
            ESP_LOGW(m_tag, "suspended");
            vTaskSuspend(NULL);
        }

        // reload config if settings changed
        if (m_reloadConfig) {
            m_reloadConfig = false;
            loadConfig();
        }

        // do we have a stratum host configured?
        // we do it here because we could reload the config after
        // it was updated on the UI and settings
        if (!strlen(m_config.host)) {
            vTaskDelay(pdMS_TO_TICKS(10000));
            continue;
        }

        // should stay stopped?
        if (m_stopFlag) {
            vTaskDelay(pdMS_TO_TICKS(10000));
            continue;
        }

        // check if wifi is connected
        // esp_wifi_connect is thread safe
        if (!isWifiConnected()) {
            ESP_LOGI(m_tag, "WiFi disconnected, attempting to reconnect...");
            esp_wifi_connect();
            vTaskDelay(pdMS_TO_TICKS(10000));
            continue;
        }

        // resolve the IP of the host
        char ip[INET_ADDRSTRLEN] = {0};
        if (!resolveHostname(m_config.host, ip, sizeof(ip))) {
            ESP_LOGE(m_tag, "%s couldn't be resolved!", m_config.host);
            vTaskDelay(pdMS_TO_TICKS(10000));
            continue;
        }

        ESP_LOGI(m_tag, "Connecting to: stratum+tcp://%s:%d (%s)", m_config.host, m_config.port, ip);

        if (!(m_sock = connectStratum(ip, m_config.port))) {
            ESP_LOGE(m_tag, "Socket unable to connect to %s:%d (errno %d)", m_config.host, m_config.port, errno);
            vTaskDelay(pdMS_TO_TICKS(10000));
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

        disconnectedCallback();
        m_isConnected = false;

        vTaskDelay(pdMS_TO_TICKS(10000)); // Delay before attempting to reconnect
    }
    vTaskDelete(NULL);
}
