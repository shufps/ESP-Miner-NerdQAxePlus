#pragma once
#include <pthread.h>

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "lwip/inet.h"

#include "stratum_task.h"

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

    PoolMode m_poolmode;                                 // default FAILOVER
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
    int getNumConnectedPools();
    virtual void loadSettings();

    // abstract
    virtual const char *getResolvedIpForSelected() const = 0;
    virtual bool isUsingFallback() = 0;

    // Get information about the currently selected pool
    virtual const char *getCurrentPoolHost() = 0;
    virtual int getCurrentPoolPort() = 0;

    virtual int getNextActivePool() = 0;

    virtual uint32_t selectAsicDiff(uint32_t poolDiff, uint32_t asicMin, uint32_t asicMax) = 0;
};
