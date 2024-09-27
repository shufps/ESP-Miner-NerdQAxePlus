#include <time.h>

#include <esp_sntp.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "lwip/dns.h"
#include "esp_task_wdt.h"

#include "connect.h"

#include "global_state.h"
#include "nvs_config.h"
#include "stratum_task.h"
#include "system.h"
#include "create_jobs_task.h"
#include "boards/board.h"
#include "asic_jobs.h"

#define PORT CONFIG_STRATUM_PORT
#define STRATUM_URL CONFIG_STRATUM_URL

#define STRATUM_PW CONFIG_STRATUM_PW
#define STRATUM_DIFFICULTY CONFIG_STRATUM_DIFFICULTY

#define BASE_DELAY_MS 5000
#define MAX_RETRY_ATTEMPTS 5

static const char *TAG = "stratum_task";
static ip_addr_t ip_Addr;

// compiler can't conclude on its own that these variables are
// set on some callback, so they need to be volatile
volatile static bool bDNSFound = false;
volatile static bool bDNSInvalid = false;

static StratumApiV1Message stratum_api_v1_message = {};

int stratum_sock;

void dns_found_cb(const char *name, const ip_addr_t *ipaddr, void *callback_arg)
{
    if (ipaddr != NULL) {
        ip4_addr_t ip4addr = ipaddr->u_addr.ip4; // Obtener la estructura ip4_addr_t
        ESP_LOGI(TAG, "IP found : %d.%d.%d.%d", ip4_addr1(&ip4addr), ip4_addr2(&ip4addr), ip4_addr3(&ip4addr),
                 ip4_addr4(&ip4addr));
        ip_Addr = *ipaddr;
    } else {
        bDNSInvalid = true;
    }
    bDNSFound = true;
}

bool is_wifi_connected()
{
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        return true;
    } else {
        return false;
    }
}

extern "C" int is_socket_connected(int socket);

void cleanQueue()
{
    ESP_LOGI(TAG, "Clean Jobs: clearing queue");
    asicJobs.cleanJobs();
}

void stratum_task(void *pvParameters)
{
    STRATUM_V1_initialize_buffer();
    char host_ip[20];
    int addr_family = 0;
    int ip_protocol = 0;
    int retry_attempts = 0;
    int delay_ms = BASE_DELAY_MS;

    const char *stratum_url = SYSTEM_MODULE.getPoolUrl();
    uint16_t port = SYSTEM_MODULE.getPoolPort();

    Board *board = SYSTEM_MODULE.getBoard();

    while (1) {
        // clear flags used by the dns callback, dns_found_cb()
        bDNSFound = false;
        bDNSInvalid = false;

        // check to see if the STRATUM_URL is an ip address already
        if (inet_pton(AF_INET, stratum_url, &ip_Addr) == 1) {
            bDNSFound = true;
        } else {
            ESP_LOGI(TAG, "Get IP for URL: %s", stratum_url);
            dns_gethostbyname(stratum_url, &ip_Addr, dns_found_cb, NULL);
            while (!bDNSFound) {
                vTaskDelay(100 / portTICK_PERIOD_MS);
            }

            if (bDNSInvalid) {
                ESP_LOGE(TAG, "DNS lookup failed for URL: %s", stratum_url);
                // set ip_Addr to 0.0.0.0 so that connect() will fail
                IP_ADDR4(&ip_Addr, 0, 0, 0, 0);
            }
        }

        // make IP address string from ip_Addr
        snprintf(host_ip, sizeof(host_ip), "%d.%d.%d.%d", ip4_addr1(&ip_Addr.u_addr.ip4), ip4_addr2(&ip_Addr.u_addr.ip4),
                 ip4_addr3(&ip_Addr.u_addr.ip4), ip4_addr4(&ip_Addr.u_addr.ip4));
        ESP_LOGI(TAG, "Connecting to: stratum+tcp://%s:%d (%s)", stratum_url, port, host_ip);

        while (1) {
            if (!is_wifi_connected()) {
                ESP_LOGI(TAG, "WiFi disconnected, attempting to reconnect...");
                esp_wifi_connect();
                vTaskDelay(10000 / portTICK_PERIOD_MS);
                // delay_ms *= 2; // Increase delay exponentially
                continue;
            }

            struct sockaddr_in dest_addr;
            dest_addr.sin_addr.s_addr = inet_addr(host_ip);
            dest_addr.sin_family = AF_INET;
            dest_addr.sin_port = htons(port);
            addr_family = AF_INET;
            ip_protocol = IPPROTO_IP;

            stratum_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
            if (stratum_sock < 0) {
                ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
                if (++retry_attempts > MAX_RETRY_ATTEMPTS) {
                    ESP_LOGE(TAG, "Max retry attempts reached, restarting...");
                    esp_restart();
                }
                vTaskDelay(5000 / portTICK_PERIOD_MS);
                continue;
            }
            ESP_LOGI(TAG, "Socket created, connecting to %s:%d", host_ip, port);

            retry_attempts = 0;
            int err = connect(stratum_sock, (struct sockaddr *) &dest_addr, sizeof(struct sockaddr_in6));
            if (err != 0) {
                ESP_LOGE(TAG, "Socket unable to connect to %s:%d (errno %d)", stratum_url, port, errno);
                // close the socket
                shutdown(stratum_sock, SHUT_RDWR);
                close(stratum_sock);
                // instead of restarting, retry this every 5 seconds
                vTaskDelay(5000 / portTICK_PERIOD_MS);
                continue;
            }

            // we add timeout to prevent recv to hang forever
            // if it times out on the recv we will check the connection state
            // and retry if still connected
            struct timeval timeout;
            timeout.tv_sec = 30; // 30 seconds timeout
            timeout.tv_usec = 0; // 0 microseconds

            ESP_LOGI(TAG, "Set socket timeout to %d for recv and write", (int) timeout.tv_sec);
            if (setsockopt(stratum_sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
                ESP_LOGE(TAG, "Failed to set socket receive timeout");
            }

            if (setsockopt(stratum_sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
                ESP_LOGE(TAG, "Failed to set socket send timeout");
            }

            STRATUM_V1_reset_uid();
            cleanQueue();

            ///// Start Stratum Action
            // mining.subscribe - ID: 1
            STRATUM_V1_subscribe(stratum_sock, board->getDeviceModel(), board->getAsicModel());

            // mining.configure - ID: 2
            STRATUM_V1_configure_version_rolling(stratum_sock);

            // mining.suggest_difficulty - ID: 3
            STRATUM_V1_suggest_difficulty(stratum_sock, STRATUM_DIFFICULTY);

            char *username = nvs_config_get_string(NVS_CONFIG_STRATUM_USER, STRATUM_USER);
            char *password = nvs_config_get_string(NVS_CONFIG_STRATUM_PASS, STRATUM_PW);

            // mining.authorize - ID: 4
            STRATUM_V1_authenticate(stratum_sock, username, password);
            free(password);
            free(username);

            while (1) {
                if (!is_socket_connected(stratum_sock)) {
                    ESP_LOGE(TAG, "Socket is not connected ...");
                    break;
                }
                char *line = STRATUM_V1_receive_jsonrpc_line(stratum_sock);
                if (!line) {
                    ESP_LOGE(TAG, "Failed to receive JSON-RPC line, reconnecting ...");
                    break;
                }
                ESP_LOGI(TAG, "rx: %s", line); // debug incoming stratum messages
                STRATUM_V1_parse(&stratum_api_v1_message, line);
                free(line);

                if (stratum_api_v1_message.method == MINING_NOTIFY) {
                    SYSTEM_MODULE.notifyNewNtime(stratum_api_v1_message.mining_notification->ntime);

                    // abandon work clears the asic job list
                    if (stratum_api_v1_message.should_abandon_work) {
                        cleanQueue();
                    }
                    create_job_mining_notify(stratum_api_v1_message.mining_notification);

                    // free notify
                    STRATUM_V1_free_mining_notify(stratum_api_v1_message.mining_notification);
                } else if (stratum_api_v1_message.method == MINING_SET_DIFFICULTY) {
                    SYSTEM_MODULE.setPoolDifficulty(stratum_api_v1_message.new_difficulty);
                    if (create_job_set_difficulty(stratum_api_v1_message.new_difficulty)) {
                        ESP_LOGI(TAG, "Set stratum difficulty: %ld", stratum_api_v1_message.new_difficulty);
                    }
                } else if (stratum_api_v1_message.method == MINING_SET_VERSION_MASK ||
                           stratum_api_v1_message.method == STRATUM_RESULT_VERSION_MASK) {
                    // 1fffe000
                    ESP_LOGI(TAG, "Set version mask: %08lx", stratum_api_v1_message.version_mask);
                    create_job_set_version_mask(stratum_api_v1_message.version_mask);
                } else if (stratum_api_v1_message.method == STRATUM_RESULT_SUBSCRIBE) {
                    ESP_LOGI(TAG, "Set enonce %s enonce2-len: %d", stratum_api_v1_message.extranonce_str, stratum_api_v1_message.extranonce_2_len);
                    create_job_set_enonce(stratum_api_v1_message.extranonce_str, stratum_api_v1_message.extranonce_2_len);
                } else if (stratum_api_v1_message.method == CLIENT_RECONNECT) {
                    ESP_LOGE(TAG, "Pool requested client reconnect ...");
                    break;
                } else if (stratum_api_v1_message.method == STRATUM_RESULT) {
                    if (stratum_api_v1_message.response_success) {
                        ESP_LOGI(TAG, "message result accepted");
                        SYSTEM_MODULE.notifyAcceptedShare();
                    } else {
                        ESP_LOGW(TAG, "message result rejected");
                        SYSTEM_MODULE.notifyRejectedShare();
                    }
                    // reset the watchdog because we received a result
                    esp_task_wdt_reset();
                } else if (stratum_api_v1_message.method == STRATUM_RESULT_SETUP) {
                    if (stratum_api_v1_message.response_success) {
                        ESP_LOGI(TAG, "setup message accepted");
                    } else {
                        ESP_LOGE(TAG, "setup message rejected");
                    }
                }
            }

            // track pool errors
            SYSTEM_MODULE.incPoolErrors();

            // shutdown and reconnect
            ESP_LOGE(TAG, "Shutdown socket ...");
            shutdown(stratum_sock, SHUT_RDWR);
            close(stratum_sock);
            vTaskDelay(1000 / portTICK_PERIOD_MS); // Delay before attempting to reconnect
        }
    }
    vTaskDelete(NULL);
}
