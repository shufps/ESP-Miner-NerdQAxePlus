#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

// #include "protocol_examples_common.h"
#include "main.h"

#include "asic_result_task.h"
#include "asic_task.h"
#include "create_jobs_task.h"
#include "esp_netif.h"
#include "http_server.h"
#include "influx_task.h"
#include "nvs_config.h"
#include "serial.h"
#include "stratum_task.h"
#include "system.h"
#include "user_input_task.h"
#include "history.h"
#include "boards/board.h"

volatile SystemModule SYSTEM_MODULE;
volatile AsicTaskModule ASIC_TASK_MODULE;
volatile PowerManagementModule POWER_MANAGEMENT_MODULE;


static const char *TAG = "bitaxe";
// static const double NONCE_SPACE = 4294967296.0; //  2^32

static void setup_wifi() {
    // pull the wifi credentials and hostname out of NVS
    char *wifi_ssid = nvs_config_get_string(NVS_CONFIG_WIFI_SSID, WIFI_SSID);
    char *wifi_pass = nvs_config_get_string(NVS_CONFIG_WIFI_PASS, WIFI_PASS);
    char *hostname = nvs_config_get_string(NVS_CONFIG_HOSTNAME, HOSTNAME);

    // copy the wifi ssid to the global state
    strncpy(SYSTEM_MODULE.ssid, wifi_ssid, sizeof(SYSTEM_MODULE.ssid));

    // init and connect to wifi
    wifi_init(wifi_ssid, wifi_pass, hostname);
    start_rest_server(NULL);
    EventBits_t result_bits = wifi_connect();

    if (result_bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to SSID: %s", wifi_ssid);
        strncpy(SYSTEM_MODULE.wifi_status, "Connected!", 20);
    } else if (result_bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to SSID: %s", wifi_ssid);

        strncpy(SYSTEM_MODULE.wifi_status, "Failed to connect", 20);
        // User might be trying to configure with AP, just chill here
        ESP_LOGI(TAG, "Finished, waiting for user input.");
        while (1) {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
        strncpy(SYSTEM_MODULE.wifi_status, "unexpected error", 20);
        // User might be trying to configure with AP, just chill here
        ESP_LOGI(TAG, "Finished, waiting for user input.");
        while (1) {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }

    free(wifi_ssid);
    free(wifi_pass);
    free(hostname);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Welcome to the bitaxe - hack the planet!");
    ESP_ERROR_CHECK(nvs_flash_init());

    if (!esp_psram_is_initialized()) {
        ESP_LOGE(TAG, "PSRAM is not available");
        return;
    }

    size_t total_psram = esp_psram_get_size();
    ESP_LOGI(TAG, "PSRAM found with %dMB", total_psram / (1024 * 1024));

    if (!history_init(board_get_asic_count())) {
        ESP_LOGE(TAG, "History couldn't be initialized");
        return;
    }

    ESP_LOGI(TAG, "Found Device Model: %s", board_get_device_model());
    ESP_LOGI(TAG, "Found Board Version: %d", board_get_version());

    uint64_t best_diff = nvs_config_get_u64(NVS_CONFIG_BEST_DIFF, 0);
    uint16_t should_self_test = nvs_config_get_u16(NVS_CONFIG_SELF_TEST, 0);
    if (should_self_test == 1 && best_diff < 1) {
        self_test(NULL);
        vTaskDelay(60 * 60 * 1000 / portTICK_PERIOD_MS);
    }

    xTaskCreate(SYSTEM_task, "SYSTEM_task", 4096, NULL, 3, NULL);

    setup_wifi();

    // set the startup_done flag
    SYSTEM_MODULE.startup_done = true;

    xTaskCreate(USER_INPUT_task, "user input", 8192, NULL, 5, NULL);

    // if a username is configured we assume we have a valid configuration and disable the AP
    const char *username = nvs_config_get_string(NVS_CONFIG_STRATUM_USER, NULL);
    if (username) {
        wifi_softap_off();
        board_load_settings();

        if (!board_init()) {
            ESP_LOGE(TAG, "error initializing board %s", board_get_device_model());
        }

        pthread_mutex_init(&ASIC_TASK_MODULE.valid_jobs_lock, NULL);
        for (int i = 0; i < MAX_ASIC_JOBS; i++) {
            ASIC_TASK_MODULE.active_jobs[i] = NULL;
            ASIC_TASK_MODULE.valid_jobs[i] = 0;
        }

        xTaskCreate(POWER_MANAGEMENT_task, "power mangement", 8192, NULL, 10, NULL);
        xTaskCreate(stratum_task, "stratum admin", 8192, NULL, 5, NULL);
        xTaskCreate(create_jobs_task, "stratum miner", 8192, NULL, 10, NULL);
        xTaskCreate(ASIC_result_task, "asic result", 8192, NULL, 15, NULL);
        xTaskCreate(influx_task, "influx", 8192, NULL, 1, NULL);
    }
}

void MINER_set_wifi_status(wifi_status_t status, uint16_t retry_count)
{
    if (status == WIFI_RETRYING) {
        snprintf(SYSTEM_MODULE.wifi_status, 20, "Retrying: %d", retry_count);
        return;
    } else if (status == WIFI_CONNECT_FAILED) {
        snprintf(SYSTEM_MODULE.wifi_status, 20, "Connect Failed!");
        return;
    }
}
