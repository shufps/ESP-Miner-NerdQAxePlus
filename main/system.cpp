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
    m_currentHashrate1m = 0.0;
    m_screenPage = 0;
    m_sharesAccepted = 0;
    m_sharesRejected = 0;
    m_duplicateHWNonces = 0;
    m_bestNonceDiff = Config::getBestDiff();
    m_bestSessionNonceDiff = 0;
    m_startTime = esp_timer_get_time();
    m_startupDone = false;
    m_poolErrors = 0;
    m_poolDifficulty = 8192;

    m_stratumConfig[0] = {
        true,
        Config::getStratumURL(),
        Config::getStratumPortNumber(),
        Config::getStratumUser(),
        Config::getStratumPass(),
        Config::isStratumEnonceSubscribe(),
    };

    m_stratumConfig[1] = {
        false,
        Config::getStratumFallbackURL(),
        Config::getStratumFallbackPortNumber(),
        Config::getStratumFallbackUser(),
        Config::getStratumFallbackPass(),
        Config::isStratumFallbackEnonceSubscribe(),
    };

    m_foundBlocks = 0;
    m_totalFoundBlocks = Config::getTotalFoundBlocks();

    // Initialize overheat flag
    m_overheated = false;

    // Initialize psu error flag
    m_psuError = false;

    // Initialize shown overlay flag and last error code
    m_showsOverlay = false;
    m_currentErrorCode = 0;

    // Set the best diff string
    suffixString(m_bestNonceDiff, m_bestDiffString, DIFF_STRING_SIZE, 0);
    suffixString(m_bestSessionNonceDiff, m_bestSessionDiffString, DIFF_STRING_SIZE, 0);

    // initialize AP state
    m_apState = false;

    // initialize connected flag
    m_wifiConnected = false;

    // Initialize the display
    m_display = new DisplayDriver();
    m_display->init(m_board);

    m_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");

    m_hostname = Config::getHostname();

    m_history = new History();
    if (!m_history->init(m_board->getAsicCount())) {
        ESP_LOGE(TAG, "history couldn't be initialized!");
    }
}

void System::updateHashrate() {}
void System::updateShares() {}
void System::updateBestDiff() {}
void System::clearDisplay() {}
void System::updateSystemInfo() {}
void System::updateEsp32Info() {}
void System::initConnection() {}

void System::updateConnection() {
    m_display->updateWifiStatus(m_wifiStatus.c_str());
}

void System::updateSystemPerformance() {}

void System::showApInformation(const char* error) {
    m_display->portalScreen(m_apSsid.c_str());
}

const std::string System::getMacAddress() {
    return connect_get_mac_addr();
}

// Function to fetch and return the RSSI (dBm) value
int System::get_wifi_rssi()
{
    wifi_ap_record_t ap_info;

    // Query the connected Access Point's information
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        ESP_LOGI("WIFI_RSSI", "Current RSSI: %d dBm", ap_info.rssi);
        return ap_info.rssi;  // Return the actual RSSI value
    } else {
        ESP_LOGE("WIFI_RSSI", "Failed to fetch RSSI");
        return -90;  // Return -90 to indicate an error
    }
}

double System::calculateNetworkDifficulty(uint32_t nBits) {
    uint32_t mantissa = nBits & 0x007fffff;  // Extract the mantissa from nBits
    uint8_t exponent = (nBits >> 24) & 0xff;  // Extract the exponent from nBits

    double target = (double)mantissa * pow(256, (exponent - 3));  // Calculate the target value
    double difficulty = (pow(2, 208) * 65535) / target;  // Calculate the difficulty

    return difficulty;
}

float System::getCurrentHashrate() {
    if (m_board->hasHashrateCounter()) {
        return HASHRATE_MONITOR.getSmoothedTotalChipHashrate();
    }
    return getCurrentHashrate10m();
}

void System::checkForBestDiff(double diff, uint32_t nbits) {
    if ((uint64_t)diff > m_bestSessionNonceDiff) {
        m_bestSessionNonceDiff = (uint64_t)diff;
        suffixString((uint64_t)diff, m_bestSessionDiffString, DIFF_STRING_SIZE, 0);
    }

    double networkDiff = calculateNetworkDifficulty(nbits);
    if (diff > networkDiff) {
        m_foundBlocks++;
        ESP_LOGI(TAG, "FOUND BLOCK!!! %f > %f", diff, networkDiff);

        // increase total found blocks counter
        m_totalFoundBlocks++;
        Config::setTotalFoundBlocks(m_totalFoundBlocks);

        discordAlerter.sendBlockFoundAlert(diff, networkDiff);
    }

    if ((uint64_t)diff <= m_bestNonceDiff) {
        return;
    }
    m_bestNonceDiff = (uint64_t)diff;

    Config::setBestDiff(m_bestNonceDiff);

    // Make the best_nonce_diff into a string
    suffixString((uint64_t)diff, m_bestDiffString, DIFF_STRING_SIZE, 0);

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
        //snprintf(buf, bufSize, "%*.*f%s", sigDigits + 1, nDigits, dval, suffix);
        if (nDigits < 0) nDigits = 0;
        snprintf(buf, bufSize, "%.*f%s", nDigits, dval, suffix);
    }
}

esp_reset_reason_t System::showLastResetReason() {
    esp_reset_reason_t reason = esp_reset_reason();
    switch (reason) {
        case ESP_RST_UNKNOWN: m_lastResetReason = "Unknown"; break;
        case ESP_RST_POWERON: m_lastResetReason = "Power on reset"; break;
        case ESP_RST_EXT: m_lastResetReason = "External reset"; break;
        case ESP_RST_SW: m_lastResetReason = "Software reset"; break;
        case ESP_RST_PANIC: m_lastResetReason = "Software panic reset"; break;
        case ESP_RST_INT_WDT: m_lastResetReason = "Interrupt watchdog reset"; break;
        case ESP_RST_TASK_WDT: m_lastResetReason = "Task watchdog reset"; break;
        case ESP_RST_WDT: m_lastResetReason = "Other watchdog reset"; break;
        case ESP_RST_DEEPSLEEP: m_lastResetReason = "Exiting deep sleep"; break;
        case ESP_RST_BROWNOUT: m_lastResetReason = "Brownout reset"; break;
        case ESP_RST_SDIO: m_lastResetReason = "SDIO reset"; break;
        default: m_lastResetReason = "Not specified"; break;
    }
    ESP_LOGI(TAG, "Reset reason: %s", m_lastResetReason.c_str());
    return reason;
}

void System::showError(const char *error_message, uint32_t error_code) {
    // is this error already shown? yes, do nothing
    if (m_showsOverlay && m_currentErrorCode == error_code) {
        return;
    }
    m_display->showError(error_message, error_code);
    m_showsOverlay = true;
    m_currentErrorCode = error_code;
}

void System::hideError() {
    if (!m_showsOverlay) {
        return;
    }
    m_display->hideError();
    m_currentErrorCode = 0;
    m_showsOverlay = false;
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

    while (!connect_is_sta_connected()) {
        if (connect_is_ap_running()) {
            showApInformation(nullptr);
            vTaskDelay(pdMS_TO_TICKS(5000));
        } else {
            updateConnection();
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    m_display->miningScreen();

    uint8_t countCycle = 10;

    // show initial 0.0.0.0
    m_display->updateIpAddress(m_ipAddress.c_str());
    int lastFoundBlocks = 0;

    while (1) {
        // update IP on the screen if it is available
        if (m_ipAddress != "") {
            m_display->updateIpAddress(m_ipAddress.c_str());
        }

        if (m_overheated) {
            showError("MINER OVERHEATED", 0x14);
        }

        if (m_psuError) {
            showError("PSU ERROR", 0x15);
        }

        // trigger the overlay only once when block is found
        if (m_foundBlocks != lastFoundBlocks && m_foundBlocks) {
            m_display->showFoundBlockOverlay();
        }
        lastFoundBlocks = m_foundBlocks;

        m_display->updateGlobalState();
        m_display->updateCurrentSettings();
        m_display->refreshScreen();

        vTaskDelay(pdMS_TO_TICKS(5000));
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

void System::countDuplicateHWNonces() {
    ++m_duplicateHWNonces;
}

void System::notifyMiningStarted() {}

void System::notifyNewNtime(uint32_t ntime) {}

void System::notifyFoundNonce(double poolDiff, int asicNr) {
    // ms timestamp
    uint64_t timestamp = esp_timer_get_time() / 1000llu;

    m_history->pushShare(poolDiff, timestamp, asicNr);

    m_currentHashrate10m = m_history->getCurrentHashrate10m();
    m_currentHashrate1m = m_history->getCurrentHashrate1m();
    updateHashrate();
}

