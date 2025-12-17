#pragma once

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "esp_transport.h"


class StratumTransport {
public:
    explicit StratumTransport(bool use_tls);
    ~StratumTransport();

    bool connect(const char* host, const char* ip, uint16_t port);
    int send(const void* data, size_t len);
    int recv(void* buf, size_t len);
    bool isConnected();
    void close();

private:
    void applyKeepAlive_();

    bool m_use_tls;
    esp_transport_handle_t m_t;
};

class TcpStratumTransport : public StratumTransport {
public:
    TcpStratumTransport() : StratumTransport(false) {}
};

class TlsStratumTransport : public StratumTransport {
public:
    TlsStratumTransport() : StratumTransport(true) {}
};
