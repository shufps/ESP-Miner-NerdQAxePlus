#pragma once

#include <ctime>
#include <stdint.h>
#include <string.h>

#include "boards/board.h"
#include "displays/displayDriver.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "freertos/queue.h"
#include "history.h"
#include "sntp.h"

// Configuration and constants
#define STRATUM_USER CONFIG_STRATUM_USER
#define DIFF_STRING_SIZE 12 // Maximum size of the difficulty string
#define MAX_ASIC_JOBS 128   // Maximum number of ASIC jobs allowed
// #define OVERHEAT_DEFAULT 70 // Default overheat threshold in degrees Celsius

class System {
  protected:
    int64_t m_startTime; // System start time (in milliseconds)

    // Display and UI
    int m_screenPage;   // Current screen page (for OLED or other displays)
    char m_oledBuf[20]; // Buffer to hold OLED display information

    // System status flags
    bool m_startupDone;     // Flag to indicate if system startup is complete

    // Network and connection info
    char m_ssid[33];       // WiFi SSID (+1 for null terminator)
    char m_wifiStatus[20]; // WiFi status string
    bool m_apState;
    char *m_hostname;
    char m_ipAddress[IP4ADDR_STRLEN_MAX] = "0.0.0.0";

    StratumConfig m_stratumConfig[2];

    // Error tracking
    Board::Error m_boardError; // Flag to indicate if the system is overheated
    uint32_t m_errorCode = 0x00000000;

    bool m_showsOverlay; // Flat if overlay is shown
    uint32_t m_currentErrorCode;

    const char *m_lastResetReason;

    History *m_history;

    // Network interface
    esp_netif_t *m_netif;         // ESP32 network interface structure
    esp_netif_ip_info_t m_ipInfo; // IP information for the network interface

    // FreeRTOS queue for handling user input
    QueueHandle_t m_userInputQueue; // Queue for managing user input events

    // display
    DisplayDriver *m_display;

    // board
    Board *m_board;

    // Internal helper methods for system management
    void initSystem();                                 // Initialize system components
    void updateHashrate();                             // Update the hashrate
    void updateBestDiff();                             // Update the best difficulty found
    void clearDisplay();                               // Clear the display
    void updateSystemInfo();                           // Update system information
    void updateEsp32Info();                            // Update ESP32-specific information
    void initConnection();                             // Initialize network connection
    void updateConnection();                           // Update connection status
    void updateSystemPerformance();                    // Update performance metrics
    void showApInformation(const char *error);         // Show Access Point (AP) information with optional error message
    double calculateNetworkDifficulty(uint32_t nBits); // Calculate network difficulty based on pool difficulty

  public:
    System();

    // Task wrapper for FreeRTOS task creation
    static void taskWrapper(void *pvParameters);

    // Main task method, typically runs the main loop
    void task();

    // hide and show error overlay
    void showError(const char *error_message, uint32_t error_code);
    void hideError();

    // Notification methods to update share statistics
    void notifyMiningStarted();                                        // Notify system that mining has started

    // WiFi related
    const char *getMacAddress();
    int get_wifi_rssi();

    int64_t getStartTime() const
    {
        return m_startTime;
    }
    double getCurrentHashrate10m() const
    {
        if (!m_history) {
            return 0.0;
        }
        return m_history->getCurrentHashrate10m();
    }

    double getCurrentHashrate1m() const
    {
        if (!m_history) {
            return 0.0;
        }
        return m_history->getCurrentHashrate1m();
    }

    float getCurrentHashrate();

    StratumConfig *getStratumConfig(uint8_t index)
    {
        return &m_stratumConfig[index];
    }

    void setBoardError(Board::Error error, uint32_t code)
    {
        m_errorCode = code;
        m_boardError = error;
    }

    // WiFi-related getters and setters
    const char *getWifiStatus() const
    {
        return m_wifiStatus;
    }
    const char *getSsid() const
    {
        return m_ssid;
    }
    void setWifiStatus(const char *wifiStatus)
    {
        strncpy(m_wifiStatus, wifiStatus, sizeof(m_wifiStatus));
    }

    void setAPState(bool state)
    {
        m_apState = state;
    }

    bool getAPState()
    {
        return m_apState;
    }

    void setSsid(const char *ssid)
    {
        strncpy(m_ssid, ssid, sizeof(m_ssid));
    }

    // Startup status setter
    void setStartupDone()
    {
        m_startupDone = true;
    }

    void setBoard(Board *board)
    {
        m_board = board;
    }

    Board *getBoard()
    {
        return m_board;
    }

    History *getHistory()
    {
        return m_history;
    }

    esp_reset_reason_t showLastResetReason();

    const char *getLastResetReason()
    {
        return m_lastResetReason;
    }

    const char *getHostname()
    {
        return (const char *) m_hostname;
    }

    const char *getIPAddress()
    {
        return (const char *) m_ipAddress;
    }

    void loadSettings();

    bool isStartupDone()
    {
        return m_startupDone;
    }

    void pushShare(int nr) {
        m_history->pushShare(nr);
    }
};
