#pragma once

#include <pthread.h>

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
    StratumConfig *m_config;

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
    bool setupSocketTimeouts();

    // stratum client loop
    void stratumLoop();

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

    void setStopFlag(bool flag)
    {
        m_stopFlag = flag;
    }

    const char *getHost() {
        return m_config->host;
    }

    int getPort() {
        return m_config->port;
    }

  public:
    StratumTask(StratumManager *manager, int index, StratumConfig *config);

    static void taskWrapper(void *pvParameters);
};

class StratumManager {
    friend StratumTask;

  protected:
    pthread_mutex_t m_mutex = PTHREAD_MUTEX_INITIALIZER;

    const char *m_tag = "stratum-manager";

    // Stratum message
    // we define it here because it's quite large for the stack
    StratumApiV1Message m_stratum_api_v1_message;

    StratumTask *m_stratumTasks[2];

    int m_selected = 0;

    void connectedCallback(int index);
    void disconnectedCallback(int index);
    void connect(int index);
    void disconnect(int index);
    void dispatch(int pool, const char *line);

    void task();

    // clear asic jobs
    void cleanQueue();


  public:
    StratumManager();

    static void taskWrapper(void *pvParameters);

    const char *getCurrentPoolHost();
    int getCurrentPoolPort();

    void submitShare(const char *jobid, const char *extranonce_2, const uint32_t ntime, const uint32_t nonce, const uint32_t version);
};