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
#include "utils.h"

static const char* TAG = "SystemModule";

System::System() {
    // NOP
}

void System::init() {
    m_screenPage = 0;
    m_startTime = esp_timer_get_time();
    m_startupDone = false;

    // Initialize overheat flag
    m_boardError = Board::Error::NONE;

    // Initialize shown overlay flag and last error code
    m_showsOverlay = false;
    m_currentErrorCode = 0;

    // Clear the ssid string
    memset(m_ssid, 0, sizeof(m_ssid));

    // Clear the wifi_status string
    memset(m_wifiStatus, 0, 20);

    // initialize AP state
    m_apState = false;

    // Initialize the display
    m_display = new DisplayDriver();
    m_display->loadSettings();

    m_hostname = Config::getHostname();

    m_history = new History();
    if (!m_history->init(m_board->getAsicCount())) {
        ESP_LOGE(TAG, "history couldn't be initialized!");
    }
}

void System::initDisplay() {
    m_display->init(m_board);
}

esp_netif_t* System::getWifiInterface() {
    return esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
}

void System::loadSettings() {
    if (m_display) {
        m_display->loadSettings();
    }
}

void System::updateConnection() {
    //m_display->updateWifiStatus(m_wifiStatus);
}

void System::updateSystemPerformance() {}

void System::showApInformation(const char* error) {
    char apSsid[13];
    generate_ssid(apSsid);
    m_display->portalScreen(apSsid);
}

const char* System::getMacAddress() {
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

float System::getCurrentHashrate() {
    if (m_board->hasHashrateCounter()) {
        return HASHRATE_MONITOR.getSmoothedTotalChipHashrate() + HASHRATE_MONITOR.getExternalHashrate();
    }
    return getCurrentHashrate10m();
}

esp_reset_reason_t System::showLastResetReason() {
    esp_reset_reason_t reason = esp_reset_reason();
    switch (reason) {
        case ESP_RST_UNKNOWN: m_lastResetReason = "SYSTEM.RESET_UNKNOWN"; break;
        case ESP_RST_POWERON: m_lastResetReason = "SYSTEM.RESET_POWERON"; break;
        case ESP_RST_EXT: m_lastResetReason = "SYSTEM.RESET_EXTERNAL"; break;
        case ESP_RST_SW: m_lastResetReason = "SYSTEM.RESET_SOFTWARE"; break;
        case ESP_RST_PANIC: m_lastResetReason = "SYSTEM.RESET_PANIC"; break;
        case ESP_RST_INT_WDT: m_lastResetReason = "SYSTEM.RESET_INT_WATCHDOG"; break;
        case ESP_RST_TASK_WDT: m_lastResetReason = "SYSTEM.RESET_TASK_WATCHDOG"; break;
        case ESP_RST_WDT: m_lastResetReason = "SYSTEM.RESET_OTHER_WATCHDOG"; break;
        case ESP_RST_DEEPSLEEP: m_lastResetReason = "SYSTEM.RESET_DEEPSLEEP"; break;
        case ESP_RST_BROWNOUT: m_lastResetReason = "SYSTEM.RESET_BROWNOUT"; break;
        case ESP_RST_SDIO: m_lastResetReason = "SYSTEM.RESET_SDIO"; break;
        default: m_lastResetReason = "SYSTEM.RESET_NOT_SPECIFIED"; break;
    }
    ESP_LOGI(TAG, "Reset reason: %s", m_lastResetReason);
    return reason;
}

void System::showError(const char *error_message, uint32_t error_code) {
    // we are already displaying an error
    if (m_showsOverlay && m_currentErrorCode != 0) {
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

void System::trigger()
{
    pthread_mutex_lock(&m_loop_mutex);
    pthread_cond_signal(&m_loop_cond);
    pthread_mutex_unlock(&m_loop_mutex);
}

void System::timerWrapper(TimerHandle_t xTimer)
{
    // Retrieve 'this' pointer from timer ID
    System *task = (System *) pvTimerGetTimerID(xTimer);
    if (!task) {
        return;
    }
    task->trigger();
}

bool System::startTimer()
{
    // Create the timer
    m_timer = xTimerCreate(TAG, pdMS_TO_TICKS(HR_INTERVAL), pdTRUE, (void *) this, timerWrapper);

    if (m_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create timer");
        return false;
    }

    // Start the timer
    if (xTimerStart(m_timer, 0) != pdPASS) {
        ESP_LOGE(TAG, "Failed to start timer");
        return false;
    }
    return true;
}

void System::pushHistory() {
    static float filteredVreg = 0.0f;
    static float filteredAsicTemp = 0.0f;
    constexpr float alpha = 0.50f; // slight smoothing

    uint64_t timestamp = esp_timer_get_time() / 1000llu;
    float hashrate = HASHRATE_MONITOR.getHashrate() + HASHRATE_MONITOR.getExternalHashrate();
    float vregTemp = POWER_MANAGEMENT_MODULE.getVRTemp();
    float asicTemp = POWER_MANAGEMENT_MODULE.getChipTempMax();

    if (!filteredVreg || !filteredAsicTemp) {
        filteredVreg = vregTemp;
        filteredAsicTemp = asicTemp;
    } else {
        filteredVreg = vregTemp * alpha + (1.0f - alpha) * filteredVreg;
        filteredAsicTemp = asicTemp * alpha + (1.0f - alpha) * filteredAsicTemp;
    }

    m_history->push(hashrate, filteredVreg, filteredAsicTemp, timestamp);
}

void System::task() {
    startTimer();

    ESP_LOGI(TAG, "SYSTEM_task started");

    // wait until splash1 and splash2 timed out
    m_display->waitForSplashs();

    wifi_mode_t wifiMode;
    esp_err_t result;

    while (!NETWORK.getPreferredIpAddr(m_ipAddress, sizeof(m_ipAddress), nullptr)) {
        // STA not connected yet -> show captive/config info
        showApInformation(nullptr);
        vTaskDelay(pdMS_TO_TICKS(5000)); // avoid flicker/spam
    }

    m_display->updateIpAddress(m_ipAddress);
    updateConnection();
    m_display->miningScreen();

    char lastIpAddress[20] = {0};

    int lastFoundBlocks = 0;

    int toggle = 1;

    while (1) {
        pthread_mutex_lock(&m_loop_mutex);
        pthread_cond_wait(&m_loop_cond, &m_loop_mutex); // Wait for the timer
        pthread_mutex_unlock(&m_loop_mutex);

        if (POWER_MANAGEMENT_MODULE.isShutdown()) {
            ESP_LOGW(TAG, "suspended");
            vTaskSuspend(NULL);
        }

        // update IP on the screen if it is available
        bool isEth = false;
        if (NETWORK.getPreferredIpAddr(m_ipAddress, sizeof(m_ipAddress), &isEth)) {
            if (strcmp(m_ipAddress, lastIpAddress) != 0) {
                ESP_LOGI(TAG, "ip address: %s", m_ipAddress);
                m_display->updateIpAddress(m_ipAddress);

                m_display->setNetworkIcon(isEth);
            }
            strncpy(lastIpAddress, m_ipAddress, sizeof(lastIpAddress));
        }


        if (m_boardError != Board::Error::NONE) {
            showError(Board::errorToStr(m_boardError), m_errorCode);
        } else {
            m_display->hideError();
        }

        uint32_t foundBlocks = STRATUM_MANAGER->getFoundBlocks();

        // trigger the overlay only once when block is found
        if (foundBlocks != lastFoundBlocks && foundBlocks) {
            m_display->showFoundBlockOverlay();
        }
        lastFoundBlocks = foundBlocks;

        // toggle
        toggle = 1-toggle;

        m_display->updateGlobalState(toggle);
        m_display->updateCurrentSettings(toggle);
        m_display->refreshScreen();

        pushHistory();
    }
}

void System::notifyMiningStarted() {}


