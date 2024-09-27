#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_task_wdt.h"

#include "serial.h"
#include "system.h"
#include "global_state.h"
#include "main.h"
#include "asic_result_task.h"
#include "asic_jobs.h"
#include "create_jobs_task.h"
#include "nvs_config.h"
#include "stratum_task.h"
#include "history.h"
#include "boards/board.h"
#include "boards/nerdqaxeplus.h"
#include "boards/nerdoctaxeplus.h"
#include "http_server.h"
#include "influx_task.h"

#define STRATUM_WATCHDOG_TIMEOUT_SECONDS    3600

System SYSTEM_MODULE;
PowerManagementModule POWER_MANAGEMENT_MODULE;

AsicJobs asicJobs;

static const char *TAG = "bitaxe";
// static const double NONCE_SPACE = 4294967296.0; //  2^32

static void setup_wifi() {
    // pull the wifi credentials and hostname out of NVS
    char *wifi_ssid = nvs_config_get_string(NVS_CONFIG_WIFI_SSID, WIFI_SSID);
    char *wifi_pass = nvs_config_get_string(NVS_CONFIG_WIFI_PASS, WIFI_PASS);
    char *hostname = nvs_config_get_string(NVS_CONFIG_HOSTNAME, HOSTNAME);

    // copy the wifi ssid to the global state
    SYSTEM_MODULE.setSsid(wifi_ssid);

    // init and connect to wifi
    wifi_init(wifi_ssid, wifi_pass, hostname);
    start_rest_server(NULL);
    EventBits_t result_bits = wifi_connect();

    if (result_bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to SSID: %s", wifi_ssid);
        SYSTEM_MODULE.setWifiStatus("Connected!");
    } else if (result_bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to SSID: %s", wifi_ssid);

        SYSTEM_MODULE.setWifiStatus("Failed to connect");
        // User might be trying to configure with AP, just chill here
        ESP_LOGI(TAG, "Finished, waiting for user input.");
        while (1) {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
        SYSTEM_MODULE.setWifiStatus("unexpected error");
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

// Function to configure the Task Watchdog Timer (TWDT)
void initWatchdog(TaskHandle_t task) {
    // Initialize the Task Watchdog Timer configuration
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = STRATUM_WATCHDOG_TIMEOUT_SECONDS * 1000,  // Convert seconds to milliseconds
        .idle_core_mask = 0,     // No specific core
        .trigger_panic = true    // Enable panic on timeout
    };

    // Initialize the Task Watchdog Timer with the configuration
    esp_err_t result = esp_task_wdt_init(&wdt_config);
    if (result != ESP_OK) {
        printf("Failed to initialize watchdog: %d\n", result);
    }

    // Add current task to the watchdog
    esp_task_wdt_add(task);  
}

extern "C" void app_main(void)
{
    // it could trigger a reset right away after reboot
    esp_task_wdt_deinit();

    ESP_LOGI(TAG, "Welcome to the bitaxe - hack the planet!");
    ESP_ERROR_CHECK(nvs_flash_init());

    // shows and saves last reset reason
    SYSTEM_MODULE.showLastResetReason();

    if (!esp_psram_is_initialized()) {
        ESP_LOGE(TAG, "PSRAM is not available");
        return;
    }

#ifdef NERDQAXEPLUS
    Board* board = new NerdQaxePlus();
#endif
#ifdef NERDOCTAXEPLUS
    Board* board = new NerdOctaxePlus();
#endif

    SYSTEM_MODULE.setBoard(board);

    size_t total_psram = esp_psram_get_size();
    ESP_LOGI(TAG, "PSRAM found with %dMB", total_psram / (1024 * 1024));

    if (!history_init(board->getAsicCount())) {
        ESP_LOGE(TAG, "History couldn't be initialized");
        return;
    }

    ESP_LOGI(TAG, "Found Device Model: %s", board->getDeviceModel());
    ESP_LOGI(TAG, "Found Board Version: %d", board->getVersion());

    uint64_t best_diff = nvs_config_get_u64(NVS_CONFIG_BEST_DIFF, 0);
    uint16_t should_self_test = nvs_config_get_u16(NVS_CONFIG_SELF_TEST, 0);
    if (should_self_test == 1 && best_diff < 1) {
        self_test(NULL);
        vTaskDelay(60 * 60 * 1000 / portTICK_PERIOD_MS);
    }

    xTaskCreate(SYSTEM_MODULE.taskWrapper, "SYSTEM_task", 4096, &SYSTEM_MODULE, 3, NULL);

    setup_wifi();

    // set the startup_done flag
    SYSTEM_MODULE.setStartupDone();

    // if a username is configured we assume we have a valid configuration and disable the AP
    const char *username = nvs_config_get_string(NVS_CONFIG_STRATUM_USER, NULL);
    if (username) {
        wifi_softap_off();
        board->loadSettings();

        if (!board->init()) {
            ESP_LOGE(TAG, "error initializing board %s", board->getDeviceModel());
        }

        xTaskCreate(POWER_MANAGEMENT_task, "power mangement", 8192, NULL, 10, NULL);

        TaskHandle_t stratum_task_handle;

        xTaskCreate(stratum_task, "stratum admin", 8192, NULL, 5, &stratum_task_handle);

        xTaskCreate(create_jobs_task, "stratum miner", 8192, NULL, 10, NULL);
        xTaskCreate(ASIC_result_task, "asic result", 8192, NULL, 15, NULL);
        xTaskCreate(influx_task, "influx", 8192, NULL, 1, NULL);

        initWatchdog(stratum_task_handle);
    }
}

void MINER_set_wifi_status(wifi_status_t status, uint16_t retry_count)
{
    if (status == WIFI_RETRYING) {
        char buf[20];
        snprintf(buf, sizeof(buf), "Retrying: %d", retry_count);
        SYSTEM_MODULE.setWifiStatus(buf);
        return;
    } else if (status == WIFI_CONNECT_FAILED) {
        SYSTEM_MODULE.setWifiStatus("Connect Failed!");
        return;
    }
}
