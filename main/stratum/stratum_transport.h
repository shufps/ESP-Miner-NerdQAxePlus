#pragma once

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>

struct esp_tls;

class StratumTransport {
public:
    virtual ~StratumTransport() = default;

    // Connect to remote host
    virtual bool connect(const char *host, const char *ip, uint16_t port) = 0;

    // Send raw data, returns number of bytes written or negative on error
    virtual ssize_t send(const void *data, size_t len) = 0;

    // Receive raw data, returns number of bytes read, 0 on timeout, negative on error
    virtual ssize_t recv(void *buf, size_t len) = 0;

    // Check if underlying connection is still alive
    virtual bool isConnected() = 0;

    // Close connection
    virtual void close() = 0;

    // Optional: expose socket fd for select(), returns -1 if not available
    virtual int getSocketFd() = 0;
};



class TcpStratumTransport : public StratumTransport {
public:
    TcpStratumTransport();
    virtual ~TcpStratumTransport();

    virtual bool connect(const char *host, const char *ip, uint16_t port);
    virtual ssize_t send(const void *data, size_t len);
    virtual ssize_t recv(void *buf, size_t len);
    virtual bool isConnected();
    virtual void close();
    virtual int getSocketFd();

private:
    int m_sock;
};



class TlsStratumTransport : public StratumTransport {
public:
    TlsStratumTransport();
    virtual ~TlsStratumTransport();

    virtual bool connect(const char *host, const char *ip, uint16_t port);
    virtual ssize_t send(const void *data, size_t len);
    virtual ssize_t recv(void *buf, size_t len);
    virtual bool isConnected();
    virtual void close();
    virtual int getSocketFd();

private:
    esp_tls *m_tls;
    int      m_sock;
};
