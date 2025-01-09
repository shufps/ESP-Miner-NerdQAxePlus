#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "esp_log.h"
#include "esp_task_wdt.h"
#include "esp_wifi.h"
#include "esp_sntp.h"
#include "lwip/dns.h"

#include "connect.h"

#include "asic_jobs.h"
#include "boards/board.h"
#include "create_jobs_task.h"
#include "global_state.h"
#include "nvs_config.h"
#include "stratum_task.h"
#include "system.h"

static const char *TAG = "stratum_task";

extern "C" int is_socket_connected(int socket);


StratumTask::StratumTask() {
    // NOP
}

bool StratumTask::isWifiConnected()
{
    wifi_ap_record_t ap_info;
    return esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK;
}

void StratumTask::cleanQueue()
{
    ESP_LOGI(TAG, "Clean Jobs: clearing queue");
    asicJobs.cleanJobs();
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
    ESP_LOGI(TAG, "Socket created, connecting to %s:%d", host_ip, port);

    int err = connect(sock, (struct sockaddr *) &dest_addr, sizeof(struct sockaddr_in6));
    if (err != 0) {
        shutdown(sock, SHUT_RDWR);
        close(sock);
        return 0;
    }

    ESP_LOGI(TAG, "Connected");

    if (!setupSocketTimeouts(sock)) {
        ESP_LOGE(TAG, "Error setting socket timeouts");
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

    ESP_LOGI(TAG, "Set socket timeout to %d for recv and write", (int) timeout.tv_sec);
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        ESP_LOGE(TAG, "Failed to set socket receive timeout");
        return false;
    }

    if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        ESP_LOGE(TAG, "Failed to set socket send timeout");
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
    STRATUM_V1_initialize_buffer();

    const char *stratumHost = SYSTEM_MODULE.getPoolUrl();
    uint16_t port = SYSTEM_MODULE.getPoolPort();

    while (1) {
        if (!isWifiConnected()) {
            ESP_LOGI(TAG, "WiFi disconnected, attempting to reconnect...");
            esp_wifi_connect();
            vTaskDelay(10000 / portTICK_PERIOD_MS);
            continue;
        }

        char ip[17] = {0};
        if (!resolveHostname(stratumHost, ip, sizeof(ip))) {
            ESP_LOGE(TAG, "%s couldn't be resolved!", stratumHost);
            vTaskDelay(10000 / portTICK_PERIOD_MS);
            continue;
        }

        ESP_LOGI(TAG, "Connecting to: stratum+tcp://%s:%d (%s)", stratumHost, port, ip);

        if (!(m_sock = connectStratum(ip, port))) {
            ESP_LOGE(TAG, "Socket unable to connect to %s:%d (errno %d)", stratumHost, port, errno);
            vTaskDelay(10000 / portTICK_PERIOD_MS);
            continue;
        }

        // stratum loop
        stratumLoop(m_sock);

        // track pool errors
        SYSTEM_MODULE.incPoolErrors();

        // shutdown and reconnect
        ESP_LOGE(TAG, "Shutdown socket ...");
        shutdown(m_sock, SHUT_RDWR);
        close(m_sock);
        vTaskDelay(1000 / portTICK_PERIOD_MS); // Delay before attempting to reconnect
    }
    vTaskDelete(NULL);
}

void StratumTask::stratumLoop(int sock)
{
    Board *board = SYSTEM_MODULE.getBoard();

    STRATUM_V1_reset_uid();
    cleanQueue();

    ///// Start Stratum Action
    // mining.subscribe - ID: 1
    STRATUM_V1_subscribe(sock, board->getDeviceModel(), board->getAsicModel());

    // mining.configure - ID: 2
    STRATUM_V1_configure_version_rolling(sock);

    char *username = nvs_config_get_string(NVS_CONFIG_STRATUM_USER, CONFIG_STRATUM_USER);
    char *password = nvs_config_get_string(NVS_CONFIG_STRATUM_PASS, CONFIG_STRATUM_PW);

    // mining.authorize - ID: 3
    STRATUM_V1_authenticate(sock, username, password);
    free(password);
    free(username);

    // mining.suggest_difficulty - ID: 4
    STRATUM_V1_suggest_difficulty(sock, CONFIG_STRATUM_DIFFICULTY);

    while (1) {
        if (!is_socket_connected(sock)) {
            ESP_LOGE(TAG, "Socket is not connected ...");
            return;
        }

        char *line = STRATUM_V1_receive_jsonrpc_line(sock);
        if (!line) {
            ESP_LOGE(TAG, "Failed to receive JSON-RPC line, reconnecting ...");
            return;
        }

        ESP_LOGI(TAG, "rx: %s", line); // debug incoming stratum messages

        StratumApiV1Message stratum_api_v1_message;
        memset(&stratum_api_v1_message, 0, sizeof(stratum_api_v1_message));

        STRATUM_V1_parse(&stratum_api_v1_message, line);
        free(line);

        switch (stratum_api_v1_message.method) {
        case MINING_NOTIFY: {
            SYSTEM_MODULE.notifyNewNtime(stratum_api_v1_message.mining_notification->ntime);

            // abandon work clears the asic job list
            if (stratum_api_v1_message.should_abandon_work) {
                cleanQueue();
            }
            create_job_mining_notify(stratum_api_v1_message.mining_notification);

            // free notify
            STRATUM_V1_free_mining_notify(stratum_api_v1_message.mining_notification);
            break;
        }

        case MINING_SET_DIFFICULTY: {
            SYSTEM_MODULE.setPoolDifficulty(stratum_api_v1_message.new_difficulty);
            if (create_job_set_difficulty(stratum_api_v1_message.new_difficulty)) {
                ESP_LOGI(TAG, "Set stratum difficulty: %ld", stratum_api_v1_message.new_difficulty);
            }
            break;
        }

        case MINING_SET_VERSION_MASK:
        case STRATUM_RESULT_VERSION_MASK: {
            ESP_LOGI(TAG, "Set version mask: %08lx", stratum_api_v1_message.version_mask);
            create_job_set_version_mask(stratum_api_v1_message.version_mask);
            break;
        }

        case STRATUM_RESULT_SUBSCRIBE: {
            ESP_LOGI(TAG, "Set enonce %s enonce2-len: %d", stratum_api_v1_message.extranonce_str,
                     stratum_api_v1_message.extranonce_2_len);
            create_job_set_enonce(stratum_api_v1_message.extranonce_str, stratum_api_v1_message.extranonce_2_len);
            break;
        }

        case CLIENT_RECONNECT: {
            ESP_LOGE(TAG, "Pool requested client reconnect ...");
            return;
        }

        case STRATUM_RESULT: {
            if (stratum_api_v1_message.response_success) {
                ESP_LOGI(TAG, "message result accepted");
                SYSTEM_MODULE.notifyAcceptedShare();
            } else {
                ESP_LOGW(TAG, "message result rejected");
                SYSTEM_MODULE.notifyRejectedShare();
            }
            // reset the watchdog because we received a result
            esp_task_wdt_reset();
            break;
        }

        case STRATUM_RESULT_SETUP: {
            if (stratum_api_v1_message.response_success) {
                ESP_LOGI(TAG, "setup message accepted");
            } else {
                ESP_LOGE(TAG, "setup message rejected");
            }
            break;
        }

        default: {
            // NOP
        }
        }
    }
}
