#pragma once

#include <pthread.h>

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "lwip/inet.h"

#include "stratum_api.h"

class StratumManager;
class StratumManagerFallback;
class StratumManagerDualPool;

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
    bool enonceSub;       ///< Flag is enonce subscription is enabled
} StratumConfig;

/**
 * @brief Stratum Task handles the connection and communication with a Stratum pool.
 */
class StratumTask {
    friend StratumManager;
    friend StratumManagerFallback;
    friend StratumManagerDualPool;

  protected:
    StratumManager *m_manager; ///< Reference to the StratumManager
    StratumConfig *m_config;   ///< Stratum configuration for the task
    int m_index;               ///< Index of the Stratum task (0 = primary, 1 = secondary)

    StratumApi m_stratumAPI; ///< API instance for Stratum communication
    const char *m_tag;       ///< Debug tag for logging

    int m_sock; ///< Socket for the Stratum connection

    bool m_isConnected; ///< Connection state flag
    bool m_stopFlag;    ///< Stop flag for the task
    bool m_firstJob;
    bool m_validNotify; // flag if the mining notify is valid

    // Connection and network-related methods
    bool isWifiConnected();                                                      ///< Check if Wi-Fi is connected
    bool resolveHostname(const char *hostname, char *ip_str, size_t ip_str_len); ///< Resolve hostname to IP
    int connectStratum(const char *host_ip, uint16_t port);                      ///< Connect to a Stratum pool
    bool setupSocketTimeouts(int sock);                                          ///< Set up socket timeouts
    char m_lastResolvedIp[INET_ADDRSTRLEN] = {0};                                ///< Last resolved IP (for use by ping_task)

    // Main Stratum loop handling communication
    void stratumLoop();
    void connect();    ///< Establish a connection to the pool
    void disconnect(); ///< Disconnect from the pool

    // Reconnection management
    TimerHandle_t m_reconnectTimer = nullptr;                        ///< FreeRTOS timer for automatic reconnection
    static void reconnectTimerCallbackWrapper(TimerHandle_t xTimer); ///< Static wrapper for FreeRTOS timer callback
    void reconnectTimerCallback(TimerHandle_t xTimer);               ///< Handles reconnection logic
    void startReconnectTimer();                                      ///< Starts the reconnect timer
    void stopReconnectTimer();                                       ///< Stops the reconnect timer

    // Connection event callbacks
    void connectedCallback();    ///< Called when a pool successfully connects
    void disconnectedCallback(); ///< Called when a pool disconnects

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
    const char *getResolvedIp() const
    {
        return m_lastResolvedIp[0] ? m_lastResolvedIp : nullptr;
    }

  public:
    StratumTask(StratumManager *manager, int index, StratumConfig *config);
    static void taskWrapper(void *pvParameters); ///< Wrapper function for task execution
};
