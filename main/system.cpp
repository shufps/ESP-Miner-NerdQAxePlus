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

#include "global_state.h"
#include "displays/displayDriver.h"
#include "system.h"
#include "i2c_master.h"
#include "nvs_config.h"
#include "influx_task.h"
#include "history.h"
#include "boards/board.h"

static const char* TAG = "SystemModule";

System::System() {
    // NOP
}

void System::initSystem() {
    m_currentHashrate10m = 0.0;
    m_screenPage = 0;
    m_sharesAccepted = 0;
    m_sharesRejected = 0;
    m_bestNonceDiff = nvs_config_get_u64(NVS_CONFIG_BEST_DIFF, 0);
    m_bestSessionNonceDiff = 0;
    m_startTime = esp_timer_get_time();
    m_lastClockSync = 0;
    m_foundBlock = false;
    m_startupDone = false;
    m_poolErrors = 0;
    m_poolDifficulty = 8192;

    // Set the pool URL
    m_poolUrl = nvs_config_get_string(NVS_CONFIG_STRATUM_URL, CONFIG_STRATUM_URL);

    // Set the pool port
    m_poolPort = nvs_config_get_u16(NVS_CONFIG_STRATUM_PORT, CONFIG_STRATUM_PORT);

    // Initialize overheat flag
    m_overheated = false;

    // Set the best diff string
    suffixString(m_bestNonceDiff, m_bestDiffString, DIFF_STRING_SIZE, 0);
    suffixString(m_bestSessionNonceDiff, m_bestSessionDiffString, DIFF_STRING_SIZE, 0);

    // Clear the ssid string
    memset(m_ssid, 0, sizeof(m_ssid));

    // Clear the wifi_status string
    memset(m_wifiStatus, 0, 20);

    // Initialize the display
    m_display.init();

    m_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
}

void System::updateHashrate() {}
void System::updateShares() {}
void System::updateBestDiff() {}
void System::clearDisplay() {}
void System::updateSystemInfo() {}
void System::updateEsp32Info() {}
void System::initConnection() {}

void System::updateConnection() {
    m_display.updateWifiStatus(m_wifiStatus);
}

void System::updateSystemPerformance() {}

void System::showApInformation(const char* error) {
    char apSsid[13];
    generate_ssid(apSsid);
    m_display.portalScreen(apSsid);
}

double System::calculateNetworkDifficulty(uint32_t nBits) {
    uint32_t mantissa = nBits & 0x007fffff;  // Extract the mantissa from nBits
    uint8_t exponent = (nBits >> 24) & 0xff;  // Extract the exponent from nBits

    double target = (double)mantissa * pow(256, (exponent - 3));  // Calculate the target value
    double difficulty = (pow(2, 208) * 65535) / target;  // Calculate the difficulty

    return difficulty;
}

void System::checkForBestDiff(double diff, uint32_t nbits) {
    if ((uint64_t)diff > m_bestSessionNonceDiff) {
        m_bestSessionNonceDiff = (uint64_t)diff;
        suffixString((uint64_t)diff, m_bestSessionDiffString, DIFF_STRING_SIZE, 0);
    }

    if ((uint64_t)diff <= m_bestNonceDiff) {
        return;
    }
    m_bestNonceDiff = (uint64_t)diff;

    nvs_config_set_u64(NVS_CONFIG_BEST_DIFF, m_bestNonceDiff);

    // Make the best_nonce_diff into a string
    suffixString((uint64_t)diff, m_bestDiffString, DIFF_STRING_SIZE, 0);

    double networkDiff = calculateNetworkDifficulty(nbits);
    if (diff > networkDiff) {
        m_foundBlock = true;
        ESP_LOGI(TAG, "FOUND BLOCK!!! %f > %f", diff, networkDiff);
    }
    ESP_LOGI(TAG, "Network diff: %f", networkDiff);
}

void System::suffixString(uint64_t val, char* buf, size_t bufSize, int sigDigits) {
    const double kKilo = 1000.0;
    const uint64_t kKiloUll = 1000ull;
    const uint64_t kMegaUll = 1000000ull;
    const uint64_t kGigaUll = 1000000000ull;
    const uint64_t kTeraUll = 1000000000000ull;
    const uint64_t kPetaUll = 1000000000000000ull;
    const uint64_t kExaUll = 1000000000000000000ull;
    char suffix[2] = "";
    bool decimal = true;
    double dval;

    if (val >= kExaUll) {
        val /= kPetaUll;
        dval = (double)val / kKilo;
        strcpy(suffix, "E");
    } else if (val >= kPetaUll) {
        val /= kTeraUll;
        dval = (double)val / kKilo;
        strcpy(suffix, "P");
    } else if (val >= kTeraUll) {
        val /= kGigaUll;
        dval = (double)val / kKilo;
        strcpy(suffix, "T");
    } else if (val >= kGigaUll) {
        val /= kMegaUll;
        dval = (double)val / kKilo;
        strcpy(suffix, "G");
    } else if (val >= kMegaUll) {
        val /= kKiloUll;
        dval = (double)val / kKilo;
        strcpy(suffix, "M");
    } else if (val >= kKiloUll) {
        dval = (double)val / kKilo;
        strcpy(suffix, "k");
    } else {
        dval = val;
        decimal = false;
    }

    if (!sigDigits) {
        if (decimal)
            snprintf(buf, bufSize, "%.3g%s", dval, suffix);
        else
            snprintf(buf, bufSize, "%d%s", (unsigned int)dval, suffix);
    } else {
        int nDigits = sigDigits - 1 - (dval > 0.0 ? floor(log10(dval)) : 0);
        snprintf(buf, bufSize, "%*.*f%s", sigDigits + 1, nDigits, dval, suffix);
    }
}

void System::showLastResetReason() {
    esp_reset_reason_t reason = esp_reset_reason();
    const char* reasonStr;
    switch (reason) {
        case ESP_RST_UNKNOWN: reasonStr = "Unknown"; break;
        case ESP_RST_POWERON: reasonStr = "Power on reset"; break;
        case ESP_RST_EXT: reasonStr = "External reset"; break;
        case ESP_RST_SW: reasonStr = "Software reset"; break;
        case ESP_RST_PANIC: reasonStr = "Software panic reset"; break;
        case ESP_RST_INT_WDT: reasonStr = "Interrupt watchdog reset"; break;
        case ESP_RST_TASK_WDT: reasonStr = "Task watchdog reset"; break;
        case ESP_RST_WDT: reasonStr = "Other watchdog reset"; break;
        case ESP_RST_DEEPSLEEP: reasonStr = "Exiting deep sleep"; break;
        case ESP_RST_BROWNOUT: reasonStr = "Brownout reset"; break;
        case ESP_RST_SDIO: reasonStr = "SDIO reset"; break;
        default: reasonStr = "Not specified"; break;
    }
    ESP_LOGI(TAG, "Reset reason: %s", reasonStr);
}

void System::taskWrapper(void* pvParameters) {
    System* systemInstance = static_cast<System*>(pvParameters);
    systemInstance->task();
}

void System::task() {
    initSystem();
    clearDisplay();
    initConnection();

    ESP_LOGI(TAG, "SYSTEM_task started");

    wifi_mode_t wifiMode;
    esp_err_t result;
    while (!m_startupDone) {
        result = esp_wifi_get_mode(&wifiMode);
        if (result == ESP_OK && (wifiMode == WIFI_MODE_APSTA || wifiMode == WIFI_MODE_AP) &&
            strcmp(m_wifiStatus, "Failed to connect") == 0) {
            showApInformation(nullptr);
            vTaskDelay(5000 / portTICK_PERIOD_MS);
        } else {
            updateConnection();
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    m_display.miningScreen();
    esp_netif_get_ip_info(m_netif, &m_ipInfo);
    char ipAddressStr[IP4ADDR_STRLEN_MAX];
    esp_ip4addr_ntoa(&m_ipInfo.ip, ipAddressStr, IP4ADDR_STRLEN_MAX);
    m_display.updateIpAddress(ipAddressStr);
    m_display.updateCurrentSettings();

    uint8_t countCycle = 10;
    bool showsOverlay = false;
    while (1) {
        if (m_overheated && !showsOverlay) {
            m_display.showOverheating();
            showsOverlay = true;
        }

        m_display.updateGlobalState();
        m_display.refreshScreen();

        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

void System::notifyAcceptedShare() {
    ++m_sharesAccepted;
    updateShares();
}

void System::notifyRejectedShare() {
    ++m_sharesRejected;
    updateShares();
}

void System::notifyMiningStarted() {}

void System::notifyNewNtime(uint32_t ntime) {
    if (m_lastClockSync + (60 * 60) > ntime) {
        return;
    }
    ESP_LOGI(TAG, "Syncing clock");
    m_lastClockSync = ntime;
    struct timeval tv;
    tv.tv_sec = ntime;
    tv.tv_usec = 0;
    settimeofday(&tv, nullptr);
}

void System::notifyFoundNonce(double poolDiff, int asicNr) {
    if (!m_lastClockSync) {
        ESP_LOGW(TAG, "clock not (yet) synchronized");
        return;
    }

    struct timeval now;
    gettimeofday(&now, nullptr);
    uint64_t timestamp = (uint64_t)now.tv_sec * 1000llu + (uint64_t)now.tv_usec / 1000llu;

    history_push_share(poolDiff, timestamp, asicNr);

    m_currentHashrate10m = history_get_current_10m();
    updateHashrate();
}
