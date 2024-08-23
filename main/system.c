#include "system.h"

#include "esp_log.h"

#include "EMC2302.h"
#include "TMP1075.h"
#include "connect.h"
#include "i2c_master.h"
#include "led_controller.h"
#include "nvs_config.h"
#include "vcore.h"

#include "driver/gpio.h"
#include "esp_app_desc.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "lwip/inet.h"

#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "influx_task.h"
#include "history.h"

#ifdef DISPLAY_TTGO
#include "displays/displayDriver.h"
#endif

static const char *TAG = "SystemModule";

static void _suffix_string(uint64_t, char *, size_t, int);

static esp_netif_t *netif;
static esp_netif_ip_info_t ip_info;

QueueHandle_t user_input_queue;

static void _init_system(GlobalState *GLOBAL_STATE)
{
    SystemModule *module = &GLOBAL_STATE->SYSTEM_MODULE;

    module->current_hashrate_10m = 0.0;
    module->screen_page = 0;
    module->shares_accepted = 0;
    module->shares_rejected = 0;
    module->best_nonce_diff = nvs_config_get_u64(NVS_CONFIG_BEST_DIFF, 0);
    module->best_session_nonce_diff = 0;
    module->start_time = esp_timer_get_time();
    module->lastClockSync = 0;
    module->FOUND_BLOCK = false;
    module->startup_done = false;
    module->pool_errors = 0;
    module->pool_difficulty = 8192;

    // set the pool url
    module->pool_url = nvs_config_get_string(NVS_CONFIG_STRATUM_URL, CONFIG_STRATUM_URL);

    // set the pool port
    module->pool_port = nvs_config_get_u16(NVS_CONFIG_STRATUM_PORT, CONFIG_STRATUM_PORT);

    // set the best diff string
    _suffix_string(module->best_nonce_diff, module->best_diff_string, DIFF_STRING_SIZE, 0);
    _suffix_string(module->best_session_nonce_diff, module->best_session_diff_string, DIFF_STRING_SIZE, 0);

    // set the ssid string to blank
    memset(module->ssid, 0, sizeof(module->ssid));

    // set the wifi_status to blank
    memset(module->wifi_status, 0, 20);

    // test the LEDs
    //  ESP_LOGI(TAG, "Init LEDs!");
    //  ledc_init();
    //  led_set();

    // Init I2C
    ESP_ERROR_CHECK(i2c_master_init());
    ESP_LOGI(TAG, "I2C initialized successfully");

    // Initialize the core voltage regulator
    VCORE_init(GLOBAL_STATE);
    VCORE_set_voltage(nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE, CONFIG_ASIC_VOLTAGE) / 1000.0, GLOBAL_STATE);

    switch (GLOBAL_STATE->device_model) {
    case DEVICE_NERDQAXE_PLUS:
        EMC2302_init(nvs_config_get_u16(NVS_CONFIG_INVERT_FAN_POLARITY, 1));
        break;
    default:
    }

    vTaskDelay(500 / portTICK_PERIOD_MS);

#ifdef DISPLAY_TTGO
    // Display TTGO-TdisplayS3
    display_init();
#endif
    netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
}

static void _show_overheat_screen(GlobalState *GLOBAL_STATE)
{
#ifdef DISPLAY_TTGO
// todo
#endif
}

static void _update_hashrate(GlobalState *GLOBAL_STATE)
{}

static void _update_shares(GlobalState *GLOBAL_STATE)
{}

static void _update_best_diff(GlobalState *GLOBAL_STATE)
{}

static void _clear_display(GlobalState *GLOBAL_STATE)
{}

static void _update_system_info(GlobalState *GLOBAL_STATE)
{}

static void _update_esp32_info(GlobalState *GLOBAL_STATE)
{}

static void _init_connection(GlobalState *GLOBAL_STATE)
{}

static void _update_connection(GlobalState *GLOBAL_STATE)
{
    SystemModule *module = &GLOBAL_STATE->SYSTEM_MODULE;
#ifdef DISPLAY_TTGO
    display_UpdateWifiStatus(module->wifi_status);
#endif
}

static void _update_system_performance(GlobalState *GLOBAL_STATE)
{}

static void show_ap_information(const char *error, GlobalState *GLOBAL_STATE)
{
    char ap_ssid[13];
    generate_ssid(ap_ssid);
#ifdef DISPLAY_TTGO
    display_PortalScreen(ap_ssid);
#endif
}

static double _calculate_network_difficulty(uint32_t nBits)
{
    uint32_t mantissa = nBits & 0x007fffff;  // Extract the mantissa from nBits
    uint8_t exponent = (nBits >> 24) & 0xff; // Extract the exponent from nBits

    double target = (double) mantissa * pow(256, (exponent - 3)); // Calculate the target value

    double difficulty = (pow(2, 208) * 65535) / target; // Calculate the difficulty

    return difficulty;
}

static void _check_for_best_diff(GlobalState *GLOBAL_STATE, double diff, uint8_t job_id)
{
    SystemModule *module = &GLOBAL_STATE->SYSTEM_MODULE;

    if ((uint64_t) diff > module->best_session_nonce_diff) {
        module->best_session_nonce_diff = (uint64_t) diff;
        _suffix_string((uint64_t) diff, module->best_session_diff_string, DIFF_STRING_SIZE, 0);
    }

    if ((uint64_t) diff <= module->best_nonce_diff) {
        return;
    }
    module->best_nonce_diff = (uint64_t) diff;

    nvs_config_set_u64(NVS_CONFIG_BEST_DIFF, module->best_nonce_diff);

    // make the best_nonce_diff into a string
    _suffix_string((uint64_t) diff, module->best_diff_string, DIFF_STRING_SIZE, 0);

    double network_diff = _calculate_network_difficulty(GLOBAL_STATE->ASIC_TASK_MODULE.active_jobs[job_id]->target);
    if (diff > network_diff) {
        module->FOUND_BLOCK = true;
        ESP_LOGI(TAG, "FOUND BLOCK!!!!!!!!!!!!!!!!!!!!!! %f > %f", diff, network_diff);
    }
    ESP_LOGI(TAG, "Network diff: %f", network_diff);
}

/* Convert a uint64_t value into a truncated string for displaying with its
 * associated suitable for Mega, Giga etc. Buf array needs to be long enough */
static void _suffix_string(uint64_t val, char *buf, size_t bufsiz, int sigdigits)
{
    const double dkilo = 1000.0;
    const uint64_t kilo = 1000ull;
    const uint64_t mega = 1000000ull;
    const uint64_t giga = 1000000000ull;
    const uint64_t tera = 1000000000000ull;
    const uint64_t peta = 1000000000000000ull;
    const uint64_t exa = 1000000000000000000ull;
    char suffix[2] = "";
    bool decimal = true;
    double dval;

    if (val >= exa) {
        val /= peta;
        dval = (double) val / dkilo;
        strcpy(suffix, "E");
    } else if (val >= peta) {
        val /= tera;
        dval = (double) val / dkilo;
        strcpy(suffix, "P");
    } else if (val >= tera) {
        val /= giga;
        dval = (double) val / dkilo;
        strcpy(suffix, "T");
    } else if (val >= giga) {
        val /= mega;
        dval = (double) val / dkilo;
        strcpy(suffix, "G");
    } else if (val >= mega) {
        val /= kilo;
        dval = (double) val / dkilo;
        strcpy(suffix, "M");
    } else if (val >= kilo) {
        dval = (double) val / dkilo;
        strcpy(suffix, "k");
    } else {
        dval = val;
        decimal = false;
    }

    if (!sigdigits) {
        if (decimal)
            snprintf(buf, bufsiz, "%.3g%s", dval, suffix);
        else
            snprintf(buf, bufsiz, "%d%s", (unsigned int) dval, suffix);
    } else {
        /* Always show sigdigits + 1, padded on right with zeroes
         * followed by suffix */
        int ndigits = sigdigits - 1 - (dval > 0.0 ? floor(log10(dval)) : 0);

        snprintf(buf, bufsiz, "%*.*f%s", sigdigits + 1, ndigits, dval, suffix);
    }
}

void showLastResetReason()
{
    // Obtener la razón del último reinicio
    esp_reset_reason_t reason = esp_reset_reason();

    // Convertir la razón del reinicio a un string legible
    const char *reason_str;
    switch (reason) {
    case ESP_RST_UNKNOWN:
        reason_str = "Unknown";
        break;
    case ESP_RST_POWERON:
        reason_str = "Power on reset";
        break;
    case ESP_RST_EXT:
        reason_str = "External reset";
        break;
    case ESP_RST_SW:
        reason_str = "Software reset";
        break;
    case ESP_RST_PANIC:
        reason_str = "Software panic reset";
        break;
    case ESP_RST_INT_WDT:
        reason_str = "Interrupt watchdog reset";
        break;
    case ESP_RST_TASK_WDT:
        reason_str = "Task watchdog reset";
        break;
    case ESP_RST_WDT:
        reason_str = "Other watchdog reset";
        break;
    case ESP_RST_DEEPSLEEP:
        reason_str = "Exiting deep sleep";
        break;
    case ESP_RST_BROWNOUT:
        reason_str = "Brownout reset";
        break;
    case ESP_RST_SDIO:
        reason_str = "SDIO reset";
        break;
    default:
        reason_str = "Not specified";
        break;
    }

    // Imprimir la razón del reinicio en el log
    ESP_LOGI(TAG, "Reset reason: %s", reason_str);
}

void SYSTEM_task(void *pvParameters)
{
    GlobalState *GLOBAL_STATE = (GlobalState *) pvParameters;
    SystemModule *module = &GLOBAL_STATE->SYSTEM_MODULE;

    _init_system(GLOBAL_STATE);
    user_input_queue = xQueueCreate(10, sizeof(char[10])); // Create a queue to handle user input events

    _clear_display(GLOBAL_STATE);
    _init_connection(GLOBAL_STATE);

    char input_event[10];
    ESP_LOGI(TAG, "SYSTEM_task started");

    while (GLOBAL_STATE->ASIC_functions.init_fn == NULL) {
#ifdef DISPLAY_TTGO
        display_log_message("ERROR > ASIC MODEL INVALID");
#endif
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }

    // At this point connection was done
#ifdef DISPLAY_TTGO
    wifi_mode_t wifi_mode;
    esp_err_t result;
    while (!module->startup_done) {
        result = esp_wifi_get_mode(&wifi_mode);
        if (result == ESP_OK && (wifi_mode == WIFI_MODE_APSTA || wifi_mode == WIFI_MODE_AP) &&
            strcmp(module->wifi_status, "Failed to connect") == 0) {
            show_ap_information(NULL, GLOBAL_STATE);
            vTaskDelay(5000 / portTICK_PERIOD_MS);
        } else {
            _update_connection(GLOBAL_STATE);
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    // show the connection screen
    display_MiningScreen(); // Create Miner Screens to be able to update its vars
    esp_netif_get_ip_info(netif, &ip_info);
    char ip_address_str[IP4ADDR_STRLEN_MAX];
    esp_ip4addr_ntoa(&ip_info.ip, ip_address_str, IP4ADDR_STRLEN_MAX);
    display_updateIpAddress(ip_address_str);
    display_updateCurrentSettings(GLOBAL_STATE);
#else
    while (!module->startup_done) {
        _update_connection(GLOBAL_STATE);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
#endif

    uint8_t countCycle = 10;
    while (1) {

#ifdef DISPLAY_TTGO
        // Display TTGO-TDISPLAYS3
        // display_updateTime(&GLOBAL_STATE->SYSTEM_MODULE);
        display_updateGlobalState(GLOBAL_STATE);
        display_RefreshScreen();

        vTaskDelay(5000 / portTICK_PERIOD_MS);
#endif
    }
}

void SYSTEM_notify_accepted_share(GlobalState *GLOBAL_STATE)
{
    SystemModule *module = &GLOBAL_STATE->SYSTEM_MODULE;

    module->shares_accepted++;
    _update_shares(GLOBAL_STATE);
}
void SYSTEM_notify_rejected_share(GlobalState *GLOBAL_STATE)
{
    SystemModule *module = &GLOBAL_STATE->SYSTEM_MODULE;

    module->shares_rejected++;
    _update_shares(GLOBAL_STATE);
}

void SYSTEM_notify_mining_started(GlobalState *GLOBAL_STATE)
{}

void SYSTEM_notify_new_ntime(GlobalState *GLOBAL_STATE, uint32_t ntime)
{
    SystemModule *module = &GLOBAL_STATE->SYSTEM_MODULE;

    // Hourly clock sync
    if (module->lastClockSync + (60 * 60) > ntime) {
        return;
    }
    ESP_LOGI(TAG, "Syncing clock");
    module->lastClockSync = ntime;
    struct timeval tv;
    tv.tv_sec = ntime;
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);
}

void SYSTEM_check_for_best_diff(GlobalState *GLOBAL_STATE, double found_diff, uint8_t job_id)
{
    _check_for_best_diff(GLOBAL_STATE, found_diff, job_id);
}

void SYSTEM_notify_found_nonce(GlobalState *GLOBAL_STATE, double pool_diff)
{
    SystemModule *module = &GLOBAL_STATE->SYSTEM_MODULE;

    if (!module->lastClockSync) {
        ESP_LOGW(TAG, "clock not (yet) synchronized");
        return;
    }

    // use stratum client if synchronized time
    struct timeval now;
    gettimeofday(&now, NULL);

    uint64_t timestamp = (uint64_t)now.tv_sec * 1000llu + (uint64_t)now.tv_usec / 1000llu;


    history_push_share(pool_diff, timestamp);


    module->current_hashrate_10m = history_get_current_10m();

    _update_hashrate(GLOBAL_STATE);
}
