#pragma once
#include <pthread.h>

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "lwip/inet.h"

#include "ArduinoJson.h"

#include "stratum_task.h"
#include "../tasks/ping_task.h"

#define DIFF_STRING_SIZE 12

/**
 * @brief StratumManager handles pool selection, connection management, and failover.
 */
class StratumManager {
    friend StratumTask;
    friend PingTask;
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
    PoolMode m_poolmode;                                 // default FAILOVER
    uint64_t m_lastSubmitResponseTimestamp;              ///< Timestamp of last submitted share response

    StratumTask *m_stratumTasks[2]{};                      ///< Primary and secondary Stratum tasks
    PingTask *m_pingTasks[2]{};
    StratumConfig m_stratumConfig[2]{};

    uint32_t m_totalFoundBlocks = 0;
    uint32_t m_foundBlocks = 0;

    // Difficulty tracking (compatibility)
    uint64_t m_totalBestDiff = 0;                       // Best nonce difficulty found
    char m_totalBestDiffString[DIFF_STRING_SIZE]{};        // String representation of the best difficulty
    char m_bestSessionDiffString[DIFF_STRING_SIZE]{}; // String representation of the best session difficulty

    bool m_initialized = false;

    PoolMode getPoolMode() const
    {
        return m_poolmode;
    }

    const StratumConfig getStratumConfig(int i);

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

    virtual void acceptedShare(int pool) = 0;
    virtual void rejectedShare(int pool) = 0;
    virtual void setPoolDifficulty(int pool, uint32_t diff) = 0;

    virtual int getPoolMode() = 0;

  public:
    StratumManager(PoolMode mode);
    static void taskWrapper(void *pvParameters); ///< Wrapper function for task execution

    // Submit shares to the active Stratum pool
    void submitShare(int pool, const char *jobid, const char *extranonce_2, const uint32_t ntime, const uint32_t nonce,
                     const uint32_t version);

    void checkForFoundBlock(int pool, double diff, uint32_t nbits);

    bool isAnyConnected();
    int getNumConnectedPools();


    virtual void getManagerInfoJson(JsonObject &obj);

    virtual void loadSettings() = 0;
    virtual void loadSettings(bool reconnect);
    virtual void saveSettings(const JsonDocument &doc);

    virtual bool isUsingFallback() {
        return false;
    }

    // abstract
    virtual const char *getResolvedIpForPool(int pool) const;

    // Get information about the currently selected pool
    virtual const char *getCurrentPoolHost() = 0;
    virtual int getCurrentPoolPort() = 0;

    virtual int getNextActivePool() = 0;

    virtual uint32_t selectAsicDiff(int pool, uint32_t poolDiff) = 0;

    virtual void checkForBestDiff(int pool, double diff, uint32_t nbits) = 0;

    bool isInitialized() {
        return m_initialized;
    }

    // compatibility
    virtual uint64_t getSharesAccepted() = 0;
    virtual uint64_t getSharesRejected() = 0;

    uint32_t getTotalFoundBlocks() {
        return m_totalFoundBlocks;
    }

    uint32_t getFoundBlocks() {
        return m_foundBlocks;
    }

    const char* getBestDiffString() {
        return m_totalBestDiffString;
    }

    virtual uint64_t getBestSessionDiff() = 0;

    const char *getBestSessionDiffString() {
        return m_bestSessionDiffString;
    }

    virtual int getPoolErrors() = 0;

    virtual uint32_t getPoolDifficulty() = 0;

    virtual int getCompatPingPoolIndex() = 0;

    PingTask *getPingTask(int i) {
        return m_pingTasks[i];
    }
};

double get_last_ping_rtt();
double get_recent_ping_loss();