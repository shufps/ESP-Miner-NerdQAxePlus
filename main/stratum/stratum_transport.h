#pragma once

#include <stddef.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "esp_transport.h"


class StratumTransport {
public:
    explicit StratumTransport(bool use_tls);
    virtual ~StratumTransport();

    virtual bool connect(const char* host, const char* ip, uint16_t port);
    virtual int send(const void* data, size_t len);
    virtual int recv(void* buf, size_t len);
    virtual bool isConnected();
    virtual void close();

private:
    bool m_use_tls;
    void applyKeepAlive_();
    void setNoDelay_();

protected:
    void shutdownSocket_();

    esp_transport_handle_t m_t;

    // Guards m_t (and the noise ctx in the derived class) between the task
    // owning the connection (connect/close) and the ASIC result task
    // submitting shares (send/isConnected). recv() is only called by the
    // owner task and stays unlocked so a blocking read can't stall submits.
    // Recursive because connect() calls close().
    pthread_mutex_t m_lock;

    // esp_transport_*_set_keep_alive() stores this pointer (no copy) and
    // dereferences it later inside esp_transport_connect(), so the config
    // must outlive applyKeepAlive_()
    esp_transport_keep_alive_t m_keepAlive = {};
};

class TcpStratumTransport : public StratumTransport {
public:
    TcpStratumTransport() : StratumTransport(false) {}
};

class TlsStratumTransport : public StratumTransport {
public:
    TlsStratumTransport() : StratumTransport(true) {}
};
