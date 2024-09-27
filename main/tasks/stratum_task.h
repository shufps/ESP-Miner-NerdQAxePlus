#pragma once


class StratumTask {
protected:
    int m_sock = 0;

    // check if wifi is connected
    bool isWifiConnected();

    // DNS resolving and connection related methods
    bool resolveHostname(const char *hostname, char *ip_str, size_t ip_str_len);
    int connectStratum(const char *host_ip, uint16_t port);
    bool setupSocketTimeouts(int sock);

    // stratum client loop
    void stratumLoop(int sock);

    // misc
    void cleanQueue();

    // task
    void task();
public:
    StratumTask();

    static void taskWrapper(void *pvParameters);

    int getStratumSock() { return m_sock; };
};


