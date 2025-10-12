#pragma once

#include <pthread.h>
#include "esp_http_server.h"

template<typename T> class MemoryGuard {
    T *&m_buf;            // reference to the caller's pointer
public:
    explicit MemoryGuard(T *&buf) : m_buf(buf) {}
    ~MemoryGuard() {
        if (m_buf) {
            free(m_buf);     // works with PSRAM too
            m_buf = nullptr;
        }
    }
};

class ClientGuard {
protected:
    esp_http_client_handle_t *m_client = nullptr;
public:
    explicit ClientGuard(esp_http_client_handle_t *client) : m_client(client) {}
    ~ClientGuard() {
        if (m_client) {
            (void)esp_http_client_close(*m_client);
            esp_http_client_cleanup(*m_client);
            m_client = nullptr;
        }
    }
};
