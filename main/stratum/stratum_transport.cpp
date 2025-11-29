#include "stratum_transport.h"

#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "esp_tls.h"
#include "mbedtls/error.h"
#include "nvs_config.h"
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string.h>
#include <sys/socket.h>

static const char *TAG_TCP = "stratum_tcp";
static const char *TAG_TLS = "stratum_tls";

static bool setup_socket_timeouts_and_keepalive(int sock)
{
    // Set recv/send timeout
    struct timeval timeout;
    timeout.tv_sec = 30;
    timeout.tv_usec = 0;

    ESP_LOGI(TAG_TCP, "Set socket timeout to %d seconds for recv and send", (int) timeout.tv_sec);

    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        ESP_LOGE(TAG_TCP, "Failed to set socket receive timeout");
        return false;
    }
    if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        ESP_LOGE(TAG_TCP, "Failed to set socket send timeout");
        return false;
    }

    int enable = Config::isStratumKeepaliveEnabled() ? 1 : 0;
    if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &enable, sizeof(enable)) < 0) {
        ESP_LOGE(TAG_TCP, "Failed to enable SO_KEEPALIVE");
        return false;
    }

    if (enable) {
        int keepidle = 10;
        int keepintvl = 5;
        int keepcnt = 3;

        if (setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(keepidle)) < 0) {
            ESP_LOGW(TAG_TCP, "TCP_KEEPIDLE not supported or failed to set");
        }
        if (setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(keepintvl)) < 0) {
            ESP_LOGE(TAG_TCP, "Failed to set TCP_KEEPINTVL");
        }
        if (setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(keepcnt)) < 0) {
            ESP_LOGE(TAG_TCP, "Failed to set TCP_KEEPCNT");
        }

        ESP_LOGI(TAG_TCP, "TCP Keepalive enabled: idle=%ds, interval=%ds, count=%d", keepidle, keepintvl, keepcnt);
    } else {
        ESP_LOGI(TAG_TCP, "TCP Keepalive is disabled via config.");
    }

    return true;
}

static bool is_fd_connected(int sock)
{
    if (sock < 0) {
        return false;
    }

    struct timeval tv;
    fd_set writefds;

    tv.tv_sec = 0;
    tv.tv_usec = 100000; // 100 ms

    FD_ZERO(&writefds);
    FD_SET(sock, &writefds);

    int ret = select(sock + 1, NULL, &writefds, NULL, &tv);
    return (ret > 0 && FD_ISSET(sock, &writefds));
}

TcpStratumTransport::TcpStratumTransport() : m_sock(-1)
{}

TcpStratumTransport::~TcpStratumTransport()
{
    close();
}

bool TcpStratumTransport::connect(const char *host, const char *ip, uint16_t port)
{
    (void) host; // hostname not needed for plain TCP
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(ip);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);

    int addr_family = AF_INET;
    int ip_protocol = IPPROTO_IP;

    m_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (m_sock < 0) {
        ESP_LOGE(TAG_TCP, "Failed to create socket");
        return false;
    }

    ESP_LOGI(TAG_TCP, "Socket created, connecting to %s:%u", ip, port);
    int err = ::connect(m_sock, (struct sockaddr *) &dest_addr, sizeof(dest_addr));
    if (err != 0) {
        ESP_LOGE(TAG_TCP, "Connect failed to %s:%u (errno %d: %s)", ip, port, errno, strerror(errno));
        close();
        return false;
    }

    ESP_LOGI(TAG_TCP, "Connected to %s:%u", ip, port);
    setup_socket_timeouts_and_keepalive(m_sock);
    return true;
}

ssize_t TcpStratumTransport::send(const void *data, size_t len)
{
    if (m_sock < 0) {
        return -1;
    }
    return ::send(m_sock, data, len, 0);
}

ssize_t TcpStratumTransport::recv(void *buf, size_t len)
{
    if (m_sock < 0) {
        return -1;
    }
    return ::recv(m_sock, buf, len, 0);
}

bool TcpStratumTransport::isConnected()
{
    return is_fd_connected(m_sock);
}

void TcpStratumTransport::close()
{
    if (m_sock >= 0) {
        ESP_LOGI(TAG_TCP, "Closing TCP socket");
        shutdown(m_sock, SHUT_RDWR);
        ::close(m_sock);
        m_sock = -1;
    }
}

int TcpStratumTransport::getSocketFd()
{
    return m_sock;
}

TlsStratumTransport::TlsStratumTransport() : m_tls(nullptr), m_sock(-1)
{}

TlsStratumTransport::~TlsStratumTransport()
{
    close();
}

bool TlsStratumTransport::connect(const char *host, const char *ip, uint16_t port)
{
    (void) ip; // TLS uses hostname for SNI and certificate verification

    // Destroy previous connection if any
    if (m_tls) {
        esp_tls_conn_destroy(m_tls);
        m_tls = nullptr;
        m_sock = -1;
    }

    esp_tls_cfg_t cfg = {};
    cfg.crt_bundle_attach = esp_crt_bundle_attach;
    cfg.timeout_ms = 30000;

    ESP_LOGI(TAG_TLS, "Opening TLS connection to %s:%u", host, port);

    // Allocate tls context
    m_tls = esp_tls_init();
    if (!m_tls) {
        ESP_LOGE(TAG_TLS, "esp_tls_init failed");
        return false;
    }

    // New sync connect API (replaces esp_tls_conn_new)
    int ret = esp_tls_conn_new_sync(host, strlen(host), port, &cfg, m_tls);
    if (ret <= 0) {
        ESP_LOGE(TAG_TLS, "esp_tls_conn_new_sync failed, ret=%d", ret);
        esp_tls_conn_destroy(m_tls);
        m_tls = nullptr;
        m_sock = -1;
        return false;
    }

    // Get underlying TCP socket fd (note the 'sockfd' name)
    esp_err_t err = esp_tls_get_conn_sockfd(m_tls, &m_sock);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_TLS, "esp_tls_get_conn_sockfd failed, sock=%d", m_sock);
        esp_tls_conn_destroy(m_tls);
        m_tls = nullptr;
        m_sock = -1;
        return false;
    }

    // Optional: apply timeouts/keepalive to underlying socket
    setup_socket_timeouts_and_keepalive(m_sock);

    ESP_LOGI(TAG_TLS, "TLS connection established (host=%s, port=%u)", host, port);
    return true;
}

ssize_t TlsStratumTransport::send(const void *data, size_t len)
{
    if (!m_tls) {
        return -1;
    }
    int ret = esp_tls_conn_write(m_tls, data, len);
    if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
        // Non-fatal, caller may retry later
        return 0;
    }
    return ret;
}

ssize_t TlsStratumTransport::recv(void *buf, size_t len)
{
    if (!m_tls) {
        return -1;
    }
    int ret = esp_tls_conn_read(m_tls, buf, len);
    if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
        // No data yet (non-blocking-like behaviour)
        return 0;
    }
    return ret;
}

bool TlsStratumTransport::isConnected()
{
    return is_fd_connected(m_sock);
}

void TlsStratumTransport::close()
{
    if (m_tls) {
        ESP_LOGI(TAG_TLS, "Closing TLS connection");
        esp_tls_conn_destroy(m_tls);
        m_tls = nullptr;
        m_sock = -1;
    } else if (m_sock >= 0) {
        // Fallback, should not be needed normally
        shutdown(m_sock, SHUT_RDWR);
        ::close(m_sock);
        m_sock = -1;
    }
}

int TlsStratumTransport::getSocketFd()
{
    return m_sock;
}
