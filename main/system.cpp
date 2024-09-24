#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_app_desc.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "lwip/inet.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "connect.h"

#include "displays/displayDriver.h"
#include "system.h"
#include "i2c_master.h"
#include "nvs_config.h"
#include "influx_task.h"
#include "history.h"
#include "boards/board.h"

static const char *TAG = "SystemModule";

static void _suffix_string(uint64_t, char *, size_t, int);

static esp_netif_t *netif;
static esp_netif_ip_info_t ip_info;

QueueHandle_t user_input_queue;

static void _init_system()
{
    SystemModule *module = &SYSTEM_MODULE;

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

    // Initialize overheat_temp
    module->overheated = false;

    // set the best diff string
    _suffix_string(module->best_nonce_diff, module->best_diff_string, DIFF_STRING_SIZE, 0);
    _suffix_string(module->best_session_nonce_diff, module->best_session_diff_string, DIFF_STRING_SIZE, 0);

    // set the ssid string to blank
    memset(module->ssid, 0, sizeof(module->ssid));

    // set the wifi_status to blank
    memset(module->wifi_status, 0, 20);

    // Display TTGO-TdisplayS3
    display_init();

    netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
}

static void _update_hashrate()
{}

static void _update_shares()
{}

static void _update_best_diff()
{}

static void _clear_display()
{}

static void _update_system_info()
{}

static void _update_esp32_info()
{}

static void _init_connection()
{}

static void _update_connection()
{
    display_UpdateWifiStatus(SYSTEM_MODULE.wifi_status);
}

static void _update_system_performance()
{}

static void show_ap_information(const char *error)
{
    char ap_ssid[13];
    generate_ssid(ap_ssid);
    display_PortalScreen(ap_ssid);
}

static double _calculate_network_difficulty(uint32_t nBits)
{
    uint32_t mantissa = nBits & 0x007fffff;  // Extract the mantissa from nBits
    uint8_t exponent = (nBits >> 24) & 0xff; // Extract the exponent from nBits

    double target = (double) mantissa * pow(256, (exponent - 3)); // Calculate the target value

    double difficulty = (pow(2, 208) * 65535) / target; // Calculate the difficulty

    return difficulty;
}

static void _check_for_best_diff(double diff, uint8_t job_id)
{
    SystemModule *module = &SYSTEM_MODULE;

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

    double network_diff = _calculate_network_difficulty(ASIC_TASK_MODULE.active_jobs[job_id]->target);
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

    SystemModule *module = &SYSTEM_MODULE;

    _init_system();
    user_input_queue = xQueueCreate(10, sizeof(char[10])); // Create a queue to handle user input events

    _clear_display();
    _init_connection();

    char input_event[10];
    ESP_LOGI(TAG, "SYSTEM_task started");

    // At this point connection was done

    wifi_mode_t wifi_mode;
    esp_err_t result;
    while (!module->startup_done) {
        result = esp_wifi_get_mode(&wifi_mode);
        if (result == ESP_OK && (wifi_mode == WIFI_MODE_APSTA || wifi_mode == WIFI_MODE_AP) &&
            strcmp(module->wifi_status, "Failed to connect") == 0) {
            show_ap_information(NULL);
            vTaskDelay(5000 / portTICK_PERIOD_MS);
        } else {
            _update_connection();
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    // show the connection screen
    display_MiningScreen(); // Create Miner Screens to be able to update its vars
    esp_netif_get_ip_info(netif, &ip_info);
    char ip_address_str[IP4ADDR_STRLEN_MAX];
    esp_ip4addr_ntoa(&ip_info.ip, ip_address_str, IP4ADDR_STRLEN_MAX);
    display_updateIpAddress(ip_address_str);
    display_updateCurrentSettings();

    uint8_t countCycle = 10;
    bool shows_overlay = false;
    while (1) {
        // Check if device is overheated
        if (module->overheated == 1 && !shows_overlay) {
            display_showOverheating();
            shows_overlay = true;
        }

        // Display TTGO-TDISPLAYS3
        // display_updateTime(&SYSTEM_MODULE);
        display_updateGlobalState();
        display_RefreshScreen();

        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

void SYSTEM_notify_accepted_share()
{
    SystemModule *module = &SYSTEM_MODULE;

    module->shares_accepted++;
    _update_shares();
}
void SYSTEM_notify_rejected_share()
{
    SystemModule *module = &SYSTEM_MODULE;

    module->shares_rejected++;
    _update_shares();
}

void SYSTEM_notify_mining_started()
{}

void SYSTEM_notify_new_ntime(uint32_t ntime)
{
    SystemModule *module = &SYSTEM_MODULE;

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

void SYSTEM_check_for_best_diff(double found_diff, uint8_t job_id)
{
    _check_for_best_diff( found_diff, job_id);
}

void SYSTEM_notify_found_nonce(double pool_diff, int asic_nr)
{
    SystemModule *module = &SYSTEM_MODULE;

    if (!module->lastClockSync) {
        ESP_LOGW(TAG, "clock not (yet) synchronized");
        return;
    }

    // use stratum client if synchronized time
    struct timeval now;
    gettimeofday(&now, NULL);

    uint64_t timestamp = (uint64_t)now.tv_sec * 1000llu + (uint64_t)now.tv_usec / 1000llu;


    history_push_share(pool_diff, timestamp, asic_nr);


    module->current_hashrate_10m = history_get_current_10m();

    _update_hashrate();
}
