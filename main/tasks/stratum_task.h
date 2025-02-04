#pragma once

#include <pthread.h>
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

class StratumManager;

typedef struct {
    bool primary;
    const char *host;
    int port;
    const char *user;
    const char *password;
} StratumConfig;

class StratumTask {
    friend StratumManager;

  protected:
    StratumConfig *m_config = nullptr;
    StratumApi m_stratumAPI;

    int m_index;

    const char *m_tag;

    // stratum socket
    int m_sock = 0;

    // manager handling connection and fallback
    StratumManager *m_manager;

    // flags
    bool m_isConnected = false;
    bool m_stopFlag = true;

    // check if wifi is connected
    bool isWifiConnected();

    // DNS resolving and connection related methods
    bool resolveHostname(const char *hostname, char *ip_str, size_t ip_str_len);
    int connectStratum(const char *host_ip, uint16_t port);
    bool setupSocketTimeouts(int sock);

    // stratum client loop
    void stratumLoop();

    void connect();
    void disconnect();

    // submit share function
    void submitShare(const char *jobid, const char *extranonce_2, const uint32_t ntime, const uint32_t nonce,
                     const uint32_t version);

    // task
    void task();

    const char *getTag()
    {
        return m_tag;
    }

    int getStratumSock()
    {
        return m_sock;
    };

    bool isConnected()
    {
        return m_isConnected;
    }

    const char *getHost() {
        if (!m_config) {
            return "-";
        }
        return m_config->host;
    }

    int getPort() {
        if (!m_config) {
            return 0;
        }
        return m_config->port;
    }

  public:
    StratumTask(StratumManager *manager, int index, StratumConfig *config);

    static void taskWrapper(void *pvParameters);
};

class StratumManager {
    friend StratumTask;

  protected:
    const char *m_tag = "stratum-manager";

    pthread_mutex_t m_mutex = PTHREAD_MUTEX_INITIALIZER;

    StratumApiV1Message *m_stratum_api_v1_message;

    StratumTask *m_stratumTasks[2] = {nullptr, nullptr};

    int m_selected = 0;

    uint64_t m_lastSubmitResponseTimestamp = 0;

    // some small helpers
    void connect(int index);
    void disconnect(int index);
    bool isConnected(int index);

    // dispatch stratum response
    void dispatch(int pool, const char *line);

    void task();

    // clear asic jobs
    void cleanQueue();

    TimerHandle_t m_reconnectTimer;
    static void reconnectTimerCallbackWrapper(TimerHandle_t xTimer);
    void reconnectTimerCallback(TimerHandle_t xTimer);

    void startReconnectTimer();
    void stopReconnectTimer();
    void connectedCallback(int index);
    void disconnectedCallback(int index);


  public:
    StratumManager();

    static void taskWrapper(void *pvParameters);

    const char *getCurrentPoolHost();
    int getCurrentPoolPort();

    void submitShare(const char *jobid, const char *extranonce_2, const uint32_t ntime, const uint32_t nonce, const uint32_t version);

    bool isUsingFallback();
};