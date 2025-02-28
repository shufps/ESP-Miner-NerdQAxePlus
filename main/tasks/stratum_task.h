#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include <pthread.h>

class StratumManager;

/**
 * @brief Configuration structure for Stratum pools
 */
typedef struct
{
    bool primary;         ///< Indicates if this is the primary pool
    const char *host;     ///< Stratum pool hostname
    int port;             ///< Stratum pool port
    const char *user;     ///< Stratum user credentials
    const char *password; ///< Stratum password credentials
} StratumConfig;

/**
 * @brief Stratum Task handles the connection and communication with a Stratum pool.
 */
class StratumTask {
    friend StratumManager; ///< Allows StratumManager to access private members

  protected:
    StratumConfig *m_config = nullptr; ///< Stratum configuration for the task
    StratumApi m_stratumAPI;           ///< API instance for Stratum communication
    int m_index;                       ///< Index of the Stratum task (0 = primary, 1 = secondary)
    const char *m_tag;                 ///< Debug tag for logging

    int m_sock = 0;            ///< Socket for the Stratum connection
    StratumManager *m_manager; ///< Reference to the StratumManager

    bool m_isConnected = false; ///< Connection state flag
    bool m_stopFlag = true;     ///< Stop flag for the task
    bool m_firstJob;

    // Connection and network-related methods
    bool isWifiConnected();                                                      ///< Check if Wi-Fi is connected
    bool resolveHostname(const char *hostname, char *ip_str, size_t ip_str_len); ///< Resolve hostname to IP
    int connectStratum(const char *host_ip, uint16_t port);                      ///< Connect to a Stratum pool
    bool setupSocketTimeouts(int sock);                                          ///< Set up socket timeouts

    // Main Stratum loop handling communication
    void stratumLoop();
    void connect();    ///< Establish a connection to the pool
    void disconnect(); ///< Disconnect from the pool

    // Submit mining shares to the pool
    void submitShare(const char *jobid, const char *extranonce_2, const uint32_t ntime, const uint32_t nonce,
                     const uint32_t version);

    // Stratum task function
    void task();

    // Getters
    const char *getTag()
    {
        return m_tag;
    }
    int getStratumSock()
    {
        return m_sock;
    }
    bool isConnected()
    {
        return m_isConnected;
    }
    const char *getHost()
    {
        return m_config ? m_config->host : "-";
    }
    int getPort()
    {
        return m_config ? m_config->port : 0;
    }

  public:
    StratumTask(StratumManager *manager, int index, StratumConfig *config);
    static void taskWrapper(void *pvParameters); ///< Wrapper function for task execution
};

/**
 * @brief StratumManager handles pool selection, connection management, and failover.
 */
class StratumManager {
    friend StratumTask; ///< Allows StratumTask to access private members

  protected:
    const char *m_tag = "stratum-manager"; ///< Debug tag for logging

    pthread_mutex_t m_mutex = PTHREAD_MUTEX_INITIALIZER; ///< Mutex for thread safety
    StratumApiV1Message *m_stratum_api_v1_message;       ///< API message handler
    StratumTask *m_stratumTasks[2] = {nullptr, nullptr}; ///< Primary and secondary Stratum tasks

    int m_selected = 0;                         ///< Tracks the currently active pool (0 = primary, 1 = secondary)
    uint64_t m_lastSubmitResponseTimestamp = 0; ///< Timestamp of last submitted share response

    // Helper methods for connection management
    void connect(int index);     ///< Connect to a specified pool (0 = primary, 1 = secondary)
    void disconnect(int index);  ///< Disconnect from a specified pool
    bool isConnected(int index); ///< Check if a pool is connected

    // Handles incoming Stratum responses
    bool dispatch(int pool, JsonDocument &doc);

    // Core Stratum management task
    void task();

    // Clears queued mining jobs
    void cleanQueue();

    // Reconnection management
    TimerHandle_t m_reconnectTimer;                                  ///< FreeRTOS timer for automatic reconnection
    static void reconnectTimerCallbackWrapper(TimerHandle_t xTimer); ///< Static wrapper for FreeRTOS timer callback
    void reconnectTimerCallback(TimerHandle_t xTimer);               ///< Handles reconnection logic
    void startReconnectTimer();                                      ///< Starts the reconnect timer
    void stopReconnectTimer();                                       ///< Stops the reconnect timer

    // Connection event callbacks
    void connectedCallback(int index);    ///< Called when a pool successfully connects
    void disconnectedCallback(int index); ///< Called when a pool disconnects

  public:
    StratumManager();
    static void taskWrapper(void *pvParameters); ///< Wrapper function for task execution

    // Get information about the currently selected pool
    const char *getCurrentPoolHost();
    int getCurrentPoolPort();

    // Submit shares to the active Stratum pool
    void submitShare(const char *jobid, const char *extranonce_2, const uint32_t ntime, const uint32_t nonce,
                     const uint32_t version);

    bool isUsingFallback(); ///< Check if the secondary (fallback) pool is in use
};
