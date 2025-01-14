#pragma once

#include <stdint.h>
#include <string.h>

#include "displays/displayDriver.h"
#include "esp_netif.h"
#include "freertos/queue.h"
#include "boards/board.h"
#include "history.h"

// Configuration and constants
#define STRATUM_USER CONFIG_STRATUM_USER
#define DIFF_STRING_SIZE 12 // Maximum size of the difficulty string
#define MAX_ASIC_JOBS 128   // Maximum number of ASIC jobs allowed
#define OVERHEAT_DEFAULT 70 // Default overheat threshold in degrees Celsius

class System {
  protected:
    // Hashrate and timing
    double m_currentHashrate10m; // Current hashrate averaged over 10 minutes
    int64_t m_startTime;         // System start time (in milliseconds)

    // Share statistics
    uint64_t m_sharesAccepted; // Number of accepted shares
    uint64_t m_sharesRejected; // Number of rejected shares

    // Display and UI
    int m_screenPage;   // Current screen page (for OLED or other displays)
    char m_oledBuf[20]; // Buffer to hold OLED display information

    // Difficulty tracking
    uint64_t m_bestNonceDiff;                       // Best nonce difficulty found
    char m_bestDiffString[DIFF_STRING_SIZE];        // String representation of the best difficulty
    uint64_t m_bestSessionNonceDiff;                // Best nonce difficulty for the current session
    char m_bestSessionDiffString[DIFF_STRING_SIZE]; // String representation of the best session difficulty

    // System status flags
    bool m_foundBlock;  // Flag indicating if a block was found
    bool m_startupDone; // Flag to indicate if system startup is complete

    // Network and connection info
    char m_ssid[33];           // WiFi SSID (+1 for null terminator)
    char m_wifiStatus[20];     // WiFi status string
    char *m_poolUrl;           // URL of the mining pool
    uint16_t m_poolPort;       // Port number of the mining pool
    uint32_t m_poolDifficulty; // Current pool difficulty

    // Error tracking
    int m_poolErrors;  // Count of errors related to the mining pool
    bool m_overheated; // Flag to indicate if the system is overheated

    const char* m_lastResetReason;

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
    void updateShares();                               // Update share statistics
    void updateBestDiff();                             // Update the best difficulty found
    void clearDisplay();                               // Clear the display
    void updateSystemInfo();                           // Update system information
    void updateEsp32Info();                            // Update ESP32-specific information
    void initConnection();                             // Initialize network connection
    void updateConnection();                           // Update connection status
    void updateSystemPerformance();                    // Update performance metrics
    void showApInformation(const char *error);         // Show Access Point (AP) information with optional error message
    double calculateNetworkDifficulty(uint32_t nBits); // Calculate network difficulty based on pool difficulty
    void suffixString(uint64_t val, char *buf, size_t bufSize, int sigDigits); // Format a value with a suffix (e.g., K, M)

  public:
    System();

    // Task wrapper for FreeRTOS task creation
    static void taskWrapper(void *pvParameters);

    // Main task method, typically runs the main loop
    void task();

    // Notification methods to update share statistics
    void notifyAcceptedShare();                              // Notify system of an accepted share
    void notifyRejectedShare();                              // Notify system of a rejected share
    void notifyFoundNonce(double poolDiff, int asicNr);      // Notify system of a found nonce
    void checkForBestDiff(double foundDiff, uint32_t nbits); // Check if the found difficulty is the best so far
    void notifyMiningStarted();                              // Notify system that mining has started
    void notifyNewNtime(uint32_t ntime);                     // Notify system of new `ntime` received from the pool

    // Getter methods for retrieving statistics
    uint64_t getSharesRejected() const
    {
        return m_sharesRejected;
    }
    uint64_t getSharesAccepted() const
    {
        return m_sharesAccepted;
    }
    const char *getBestDiffString() const
    {
        return m_bestDiffString;
    }
    const char *getBestSessionDiffString() const
    {
        return m_bestSessionDiffString;
    }
    uint64_t getBestSessionNonceDiff() const
    {
        return m_bestSessionNonceDiff;
    }
    int64_t getStartTime() const
    {
        return m_startTime;
    }
    double getCurrentHashrate10m() const
    {
        return m_currentHashrate10m;
    }

    // Pool-related getters and setters
    const char *getPoolUrl() const
    {
        return m_poolUrl;
    }
    uint16_t getPoolPort() const
    {
        return m_poolPort;
    }
    void setPoolDifficulty(uint32_t difficulty)
    {
        m_poolDifficulty = difficulty;
    }
    uint32_t getPoolDifficulty() const
    {
        return m_poolDifficulty;
    }
    void incPoolErrors()
    {
        ++m_poolErrors;
    }
    int getPoolErrors() const
    {
        return m_poolErrors;
    }

    // Overheating status setters
    void setOverheated(bool status)
    {
        m_overheated = status;
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
    void setSsid(const char *ssid)
    {
        strncpy(m_ssid, ssid, sizeof(m_ssid));
    }

    // Block status and clock sync getters
    bool isFoundBlock() const
    {
        return m_foundBlock;
    }

    // Startup status setter
    void setStartupDone()
    {
        m_startupDone = true;
    }

    void setBoard(Board* board) {
        m_board = board;
    }

    Board* getBoard() {
        return m_board;
    }

    History* getHistory() {
        return m_history;
    }

    void showLastResetReason();

    const char* getLastResetReason() {
        return m_lastResetReason;
    }

};
