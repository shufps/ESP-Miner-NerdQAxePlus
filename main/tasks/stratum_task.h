#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "lwip/inet.h"
#include <pthread.h>

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

/**
 * @brief StratumManager handles pool selection, connection management, and failover.
 */
class StratumManager {
    friend StratumTask; ///< Allows StratumTask to access private members
  public:
    enum Selected
    {
        PRIMARY = 0,
        SECONDARY = 1
    };

    enum PoolMode
    {
        FAILOVER = 0,
        DUAL = 1
    };

  protected:
    const char *m_tag = "stratum-manager"; ///< Debug tag for logging

    pthread_mutex_t m_mutex = PTHREAD_MUTEX_INITIALIZER; ///< Mutex for thread safety
    StratumApiV1Message m_stratum_api_v1_message;        ///< API message handler
    StratumTask *m_stratumTasks[2];                      ///< Primary and secondary Stratum tasks
    PoolMode m_poolmode;                                      // default FAILOVER
    uint64_t m_lastSubmitResponseTimestamp;              ///< Timestamp of last submitted share response

    PoolMode getPoolMode() const
    {
        return m_poolmode;
    }

    // Helper methods for connection management
    void connect(int index);     ///< Connect to a specified pool (0 = primary, 1 = secondary)
    void disconnect(int index);  ///< Disconnect from a specified pool
    bool isConnected(int index); ///< Check if a pool is connected

    // Handles incoming Stratum responses
    void dispatch(int pool, JsonDocument &doc);

    // Core Stratum management task
    void task();

    // Clears queued mining jobs
    void cleanQueue();

    void freeStratumV1Message(StratumApiV1Message *message);

    // abstract methods
    // reconnect logic for failover mode
    virtual void reconnectTimerCallback(int index) = 0;
    virtual void connectedCallback(int index) = 0;
    virtual void disconnectedCallback(int index) = 0;
    virtual bool acceptsNotifyFrom(int pool) = 0;

  public:
    StratumManager(PoolMode mode);
    static void taskWrapper(void *pvParameters); ///< Wrapper function for task execution

    // Submit shares to the active Stratum pool
    void submitShare(int pool, const char *jobid, const char *extranonce_2, const uint32_t ntime, const uint32_t nonce,
                     const uint32_t version);

    bool isAnyConnected();

    // abstract
    virtual const char *getResolvedIpForSelected() const = 0;
    virtual bool isUsingFallback() = 0;

    // Get information about the currently selected pool
    virtual const char *getCurrentPoolHost() = 0;
    virtual int getCurrentPoolPort() = 0;

    virtual int getNextActivePool() = 0;
};

class StratumManagerFallback : public StratumManager {
    friend StratumTask; ///< Allows StratumTask to access private members

  protected:
    int m_selected; // default PRIMARY

    virtual void reconnectTimerCallback(int index);
    virtual void connectedCallback(int index);
    virtual void disconnectedCallback(int index);

    virtual bool acceptsNotifyFrom(int pool);

  public:
    StratumManagerFallback();

    virtual const char *getCurrentPoolHost();
    virtual int getCurrentPoolPort();

    virtual const char *getResolvedIpForSelected() const;

    virtual int getNextActivePool();

    virtual bool isUsingFallback()
    {
        return m_selected == StratumManager::Selected::SECONDARY;
    }
};

class StratumManagerDualPool : public StratumManager {
    friend StratumTask; ///< Allows StratumTask to access private members

  protected:
    int m_primary_pct = 50;
    int32_t m_error_accum = 0;

    virtual void reconnectTimerCallback(int index);
    virtual void connectedCallback(int index);
    virtual void disconnectedCallback(int index);

    virtual bool acceptsNotifyFrom(int pool);

  public:
    StratumManagerDualPool();

    virtual const char *getCurrentPoolHost();
    virtual int getCurrentPoolPort();

    virtual const char *getResolvedIpForSelected() const;

    virtual int getNextActivePool();

    virtual bool isUsingFallback()
    {
        // not applicable
        return false;
    }
};
