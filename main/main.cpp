#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_task_wdt.h"
#include "mbedtls/platform.h"
#include "nvs_flash.h"

#include "asic_jobs.h"
#include "asic_result_task.h"
#include "boards/board.h"
#include "boards/nerdoctaxeplus.h"
#include "boards/nerdoctaxegamma.h"
#include "boards/nerdqaxeplus.h"
#include "boards/nerdqaxeplus2.h"
#include "boards/nerdaxe.h"
#include "boards/nerdaxegamma.h"
#include "boards/nerdhaxegamma.h"
#include "boards/nerdeko.h"
#include "create_jobs_task.h"
#include "global_state.h"
#include "history.h"
#include "http_server.h"
#include "influx_task.h"
#include "main.h"
#include "nvs_config.h"
#include "serial.h"
#include "stratum_task.h"
#include "system.h"
#include "apis_task.h"
#include "ping_task.h"
#include "wifi_health.h"
#include "discord.h"
#include "macros.h"

#define STRATUM_WATCHDOG_TIMEOUT_SECONDS 3600

System SYSTEM_MODULE;

PowerManagementTask POWER_MANAGEMENT_MODULE;
StratumManager STRATUM_MANAGER;
APIsFetcher APIs_FETCHER;

DiscordAlerter discordAlerter;

AsicJobs asicJobs;

static const char *TAG = "nerd*axe";

#ifndef CONFIG_SPIRAM
#error "firmware will not work without psram"
#endif

// Function to configure the Task Watchdog Timer (TWDT)
void initWatchdog()
{
    // Initialize the Task Watchdog Timer configuration
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = STRATUM_WATCHDOG_TIMEOUT_SECONDS * 1000, // Convert seconds to milliseconds
        .idle_core_mask = 0,                                   // No specific core
        .trigger_panic = true                                  // Enable panic on timeout
    };

    // Initialize the Task Watchdog Timer with the configuration
    esp_err_t result = esp_task_wdt_init(&wdt_config);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize watchdog: %d\n", result);
    }
}

// Custom calloc function that allocates from PSRAM
void *psram_calloc(size_t num, size_t size) {
    void *ptr = CALLOC(num, size);
    if (!ptr) {
        ESP_LOGE(TAG, "PSRAM allocation failed! Falling back to internal RAM.");
        ptr = heap_caps_calloc(num, size, MALLOC_CAP_DEFAULT);
    }
    return ptr;
}

void free_psram(void *ptr) {
    heap_caps_free(ptr);
}

#if 0
const UBaseType_t max_tasks = 30;
TaskStatus_t task_list[max_tasks];

void monitor_all_task_watermarks() {
    UBaseType_t count = uxTaskGetSystemState(task_list, max_tasks, NULL);

    for (int i = 0; i < count; ++i) {
        ESP_LOGW("TASK_MON", "Task '%s': watermark = %lu words",
            task_list[i].pcTaskName,
            task_list[i].usStackHighWaterMark);

    }
}
#endif


extern "C" void app_main(void)
{
    initWatchdog();

    // use PSRAM because TLS costs a lot of internal RAM
    mbedtls_platform_set_calloc_free(psram_calloc, free_psram);

    ESP_LOGI(TAG, "Welcome to the Nerd*Axe - hack the planet!");
    ESP_ERROR_CHECK(nvs_flash_init());

    // shows and saves last reset reason
    esp_reset_reason_t reason = SYSTEM_MODULE.showLastResetReason();

    // migrate config
    Config::migrate_config();

#ifdef NERDQAXEPLUS
    Board *board = new NerdQaxePlus();
#endif
#ifdef NERDQAXEPLUS2
    Board *board = new NerdQaxePlus2();
#endif
#ifdef NERDOCTAXEPLUS
    Board *board = new NerdOctaxePlus();
#endif
#ifdef NERDAXE
    Board *board = new NerdAxe();
#endif
#ifdef NERDOCTAXEGAMMA
    Board *board = new NerdOctaxeGamma();
#endif
#ifdef NERDAXEGAMMA
    Board *board = new NerdaxeGamma();
#endif
#ifdef NERDHAXEGAMMA
    Board *board = new NerdHaxeGamma();
#endif
#ifdef NERDEKO
    Board *board = new NerdEko();
#endif

    // initialize everything non-asic-specific like
    // fan and serial and load settings from nvs
    board->loadSettings();
    board->initBoard();


    SYSTEM_MODULE.setBoard(board);

    size_t total_psram = esp_psram_get_size();
    ESP_LOGI(TAG, "PSRAM found with %dMB", total_psram / (1024 * 1024));
    ESP_LOGI(TAG, "Found Device Model: %s", board->getDeviceModel());
    ESP_LOGI(TAG, "Found Board Version: %d", board->getVersion());

    uint64_t best_diff = Config::getBestDiff();
    bool should_self_test = Config::isSelfTestEnabled();
    if (should_self_test && !best_diff) {
        board->selfTest();
        vTaskDelay(pdMS_TO_TICKS(60 * 60 * 1000));
    }

    xTaskCreate(SYSTEM_MODULE.taskWrapper, "SYSTEM_task", 4096, &SYSTEM_MODULE, 3, NULL);
    xTaskCreate(POWER_MANAGEMENT_MODULE.taskWrapper, "power mangement", 8192, (void *) &POWER_MANAGEMENT_MODULE, 10, NULL);

    // init AP and connect to wifi
    wifi_init();

    // set the startup_done flag
    SYSTEM_MODULE.setStartupDone();

    //start rest server
    start_rest_server(NULL);

    while (!SYSTEM_MODULE.isWifiConnected()) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    discordAlerter.init();
    discordAlerter.loadConfig();

    // we only use alerting if we are in a normal operating mode
    if (reason == ESP_RST_TASK_WDT) {
        discordAlerter.sendWatchdogAlert();
    }

    // and continue with initialization
    POWER_MANAGEMENT_MODULE.lock();
    if (!board->initAsics()) {
        ESP_LOGE(TAG, "error initializing board %s", board->getDeviceModel());
    }
    POWER_MANAGEMENT_MODULE.unlock();

    TaskHandle_t stratum_manager_handle;

    xTaskCreate(STRATUM_MANAGER.taskWrapper, "stratum manager", 8192, (void *) &STRATUM_MANAGER, 5, &stratum_manager_handle);
    xTaskCreate(create_jobs_task, "stratum miner", 8192, NULL, 10, NULL);
    xTaskCreate(ASIC_result_task, "asic result", 8192, NULL, 15, NULL);
    xTaskCreate(influx_task, "influx", 8192, NULL, 1, NULL);
    xTaskCreate(APIs_FETCHER.taskWrapper, "apis ticker", 4096, (void*) &APIs_FETCHER, 5, NULL);
    xTaskCreate(ping_task, "ping task", 4096, NULL, 1, NULL);
    xTaskCreate(wifi_monitor_task, "wifi monitor", 4096, NULL, 1, NULL);

    //char* taskList = (char*) malloc(8192);
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        size_t free_internal_heap = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);

        if (free_internal_heap < 10000) {
            ESP_LOGW(TAG, "*** WARNING *** Free internal heap: %d bytes", free_internal_heap);
        }
        //monitor_all_task_watermarks();
    }
}

