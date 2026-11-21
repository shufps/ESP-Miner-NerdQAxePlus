#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>

#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "global_state.h"
#include "esp_netif.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "asic_jobs.h"
#include "boards/board.h"
#include "connect.h"
#include "create_jobs_task.h"
#include "global_state.h"
#include "macros.h"
#include "nvs_config.h"
#include "psram_allocator.h"
#include "stratum_task.h"
#include "system.h"
#include "guards.h"

#define ESP_LOGIE(b, tag, fmt, ...)                                                                                                \
    do {                                                                                                                           \
        if (b) {                                                                                                                   \
            ESP_LOGI(tag, fmt, ##__VA_ARGS__);                                                                                     \
        } else {                                                                                                                   \
            ESP_LOGE(tag, fmt, ##__VA_ARGS__);                                                                                     \
        }                                                                                                                          \
    } while (0)

// fallback can nicely be tested with netcat
// socat TCP-LISTEN:5555,fork TCP:solo.ckpool.org:3333

// ============================================================================
// StratumTaskBase
// ============================================================================

StratumTaskBase::StratumTaskBase(StratumManager *manager, int index)
    : m_manager(manager), m_index(index)
{
    if (!index) {
        m_tag = "stratum task (Pri)";
    } else {
        m_tag = "stratum task (Sec)";
    }

    m_config = new StratumConfig(index);
}

bool StratumTaskBase::isWifiConnected()
{
    return NETWORK.hasWifiIp() || NETWORK.hasEthIp();
}

// True if the station or Ethernet interface has a preferred global or
// unique-local IPv6 address, so IPv6 pool connections can be routed.
static bool hasRoutableIpv6()
{
    const char *ifkeys[] = {"WIFI_STA_DEF", "ETH_DEF"};
    for (const char *key : ifkeys) {
        esp_netif_t *netif = esp_netif_get_handle_from_ifkey(key);
        if (!netif) {
            continue;
        }
        esp_ip6_addr_t addrs[LWIP_IPV6_NUM_ADDRESSES];
        int count = esp_netif_get_all_preferred_ip6(netif, addrs);
        for (int i = 0; i < count; i++) {
            esp_ip6_addr_type_t type = esp_netif_ip6_get_addr_type(&addrs[i]);
            if (type == ESP_IP6_ADDR_IS_GLOBAL || type == ESP_IP6_ADDR_IS_UNIQUE_LOCAL) {
                return true;
            }
        }
    }
    return false;
}

// Returns host without the brackets of an "[IPv6 literal]", else host unchanged.
const char *StratumTaskBase::stripBrackets(const char *host)
{
    size_t len = strlen(host);
    if (len < 3 || host[0] != '[' || host[len - 1] != ']' || len - 2 >= sizeof(m_hostBuf)) {
        return host;
    }
    memcpy(m_hostBuf, host + 1, len - 2);
    m_hostBuf[len - 2] = '\0';
    return m_hostBuf;
}

// IPv6 is skipped only for the host and port whose IPv6 connect failed, so
// switching pools tries IPv6 right away.
bool StratumTaskBase::isIpv6Skipped(const char *host, int port)
{
    return esp_timer_get_time() < m_ipv6RetryAfterUs && port == m_ipv6SkipPort && !strcmp(host, m_ipv6SkipHost);
}

void StratumTaskBase::skipIpv6(const char *host, int port)
{
    snprintf(m_ipv6SkipHost, sizeof(m_ipv6SkipHost), "%s", host);
    m_ipv6SkipPort = port;
    m_ipv6RetryAfterUs = esp_timer_get_time() + 5 * 60 * 1000000LL;
}

bool StratumTaskBase::resolveHostname(const char *hostname, int family, char *ip_str, size_t ip_str_len)
{
    struct addrinfo hints;
    struct addrinfo *res;
    int err;

    // Set up the hints for the lookup
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = family;

    // Perform the blocking DNS lookup
    err = getaddrinfo(hostname, NULL, &hints, &res);
    if (err != 0 || res == NULL) {
        if (family == AF_INET6) {
            ESP_LOGD("DNS", "No IPv6 address for %s", hostname);
        } else {
            ESP_LOGE("DNS", "DNS lookup failed: %d", err);
        }
        return false;
    }

    // Extract the IP address from the result
    if (res->ai_family == AF_INET6) {
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *) res->ai_addr;
        inet_ntop(AF_INET6, &(addr6->sin6_addr), ip_str, ip_str_len);
    } else {
        struct sockaddr_in *addr = (struct sockaddr_in *) res->ai_addr;
        inet_ntop(AF_INET, &(addr->sin_addr), ip_str, ip_str_len);
    }

    // save IP adrdress
    strncpy(m_lastResolvedIp, ip_str, sizeof(m_lastResolvedIp));
    m_lastResolvedIp[sizeof(m_lastResolvedIp) - 1] = '\0';

    ESP_LOGI("DNS", "Resolved IP: %s", ip_str);

    // Free the result
    freeaddrinfo(res);

    return true;
}

void StratumTaskBase::connect()
{
    m_stopFlag = false;
}

void StratumTaskBase::disconnect()
{
    m_stopFlag = true;
}

void StratumTaskBase::triggerReconnect() {
    m_reconnect = true;
}

void StratumTaskBase::reconnectTimerCallbackWrapper(TimerHandle_t xTimer)
{
    StratumTaskBase *self = static_cast<StratumTaskBase *>(pvTimerGetTimerID(xTimer));
    self->reconnectTimerCallback(xTimer);
}

// Reconnect Timer Callback
void StratumTaskBase::reconnectTimerCallback(TimerHandle_t xTimer)
{
    m_manager->reconnectTimerCallback(m_index);
}

// Connected Callback
void StratumTaskBase::connectedCallback()
{
    m_manager->connectedCallback(m_index);
}

// Disconnected Callback
void StratumTaskBase::disconnectedCallback()
{
    m_manager->disconnectedCallback(m_index);
}

// Start the reconnect timer
void StratumTaskBase::startReconnectTimer()
{
    if (m_reconnectTimer == NULL) {
        m_reconnectTimer = xTimerCreate("Reconnect Timer", pdMS_TO_TICKS(30000), pdTRUE, this, reconnectTimerCallbackWrapper);
    }

    // only start the timer when it is not running
    if (xTimerIsTimerActive(m_reconnectTimer) == pdFALSE) {
        xTimerStart(m_reconnectTimer, 0);
    }
}

// Stop the reconnect timer
void StratumTaskBase::stopReconnectTimer()
{
    if (m_reconnectTimer != NULL) {
        xTimerStop(m_reconnectTimer, 0);
    }
}

void StratumTaskBase::taskWrapper(void *pvParameters)
{
    StratumTaskBase *task = (StratumTaskBase *) pvParameters;
    task->task();
}

void StratumTaskBase::task()
{
    // Start the reconnect timer
    startReconnectTimer();

    while (1) {
        if (POWER_MANAGEMENT_MODULE.isShutdown()) {
            ESP_LOGW(m_tag, "suspended");
            vTaskSuspend(NULL);
        }

        // gets a guaranteed consistent copy of the config
        m_manager->copyConfigInto(m_index, m_config);

        m_reconnect = false;

        // do we have a stratum host configured?
        // we do it here because we could reload the config after
        // it was updated on the UI and settings
        if (!strlen(m_config->getHost())) {
            vTaskDelay(pdMS_TO_TICKS(10000));
            continue;
        }

        // should stay stopped?
        if (m_stopFlag) {
            vTaskDelay(pdMS_TO_TICKS(10000));
            continue;
        }

        // check if any network interface has an IP (WiFi or ETH)
        if (!isWifiConnected()) {
            ESP_LOGI(m_tag, "No network connection, waiting...");
            vTaskDelay(pdMS_TO_TICKS(10000));
            continue;
        }

        // select transport (protocol-specific)
        m_transport = selectTransport();

        // An IPv6 literal may be written in brackets, as in URLs
        const char *host = stripBrackets(m_config->getHost());
        int port = m_config->getPort();
        ip_addr_t literal;
        bool isLiteral = ipaddr_aton(host, &literal);
        char ip[INET6_ADDRSTRLEN] = {0};
        bool connected = false;

        // Prefer IPv6 for hostnames with an AAAA record when the device has a
        // routable IPv6 address. If IPv6 can't connect, try IPv4 for the same
        // host and skip IPv6 for this host for a while. TLS still verifies the
        // certificate against the hostname.
        if (!isLiteral && !isIpv6Skipped(host, port) && hasRoutableIpv6() &&
            resolveHostname(host, AF_INET6, ip, sizeof(ip))) {
            ESP_LOGI(m_tag, "Connecting to: stratum+tcp://%s:%d ([%s])", host, port, ip);
            connected = m_transport->connect(host, ip, port);
            if (!connected) {
                ESP_LOGW(m_tag, "IPv6 connection to %s failed, trying IPv4", host);
                skipIpv6(host, port);
            }
        }

        if (!connected) {
            // IP literals may be IPv4 or IPv6; hostnames resolve to IPv4 as before.
            // A hostname without an A record (IPv6-only pool) still uses its AAAA.
            if (!resolveHostname(host, isLiteral ? AF_UNSPEC : AF_INET, ip, sizeof(ip)) &&
                (isLiteral || !resolveHostname(host, AF_INET6, ip, sizeof(ip)))) {
                ESP_LOGE(m_tag, "%s couldn't be resolved!", host);
                vTaskDelay(pdMS_TO_TICKS(10000));
                continue;
            }

            // IPv6 addresses are shown in brackets, as in URLs
            bool v6 = strchr(ip, ':') != nullptr;
            ESP_LOGI(m_tag, "Connecting to: stratum+tcp://%s%s%s:%d (%s%s%s)", isLiteral && v6 ? "[" : "", host,
                     isLiteral && v6 ? "]" : "", port, v6 ? "[" : "", ip, v6 ? "]" : "");

            if (!m_transport->connect(host, ip, port)) {
                ESP_LOGE(m_tag, "Socket unable to connect to %s:%d (errno %d)", host, port, errno);
                vTaskDelay(pdMS_TO_TICKS(10000));
                continue;
            }
        }

        // we are connected but it doesn't mean the server is alive ...

        // protocol-specific loop
        protocolLoop();

        // track pool errors
        // reconnect request is not an error
        if (!m_reconnect) {
            m_poolErrors++;
        }

        // gate new submits before tearing the transport down; the
        // transport's internal lock covers a submit already inside send()
        m_isConnected = false;

        // shutdown and reconnect
        ESP_LOGIE(m_reconnect, m_tag, "Shutdown socket ...");
        m_transport->close();

        disconnectedCallback();

        // skip reconnect delay
        if (m_reconnect) {
            continue;
        }
        vTaskDelay(pdMS_TO_TICKS(10000)); // Delay before attempting to reconnect
    }
    vTaskDelete(NULL);
}

// ============================================================================
// StratumTaskV1
// ============================================================================

StratumTaskV1::StratumTaskV1(StratumManager *manager, int index)
    : StratumTaskBase(manager, index)
{
}

StratumTransport* StratumTaskV1::selectTransport()
{
    if (m_config->isTLS()) {
        return &m_tlsTransport;
    }
    return &m_tcpTransport;
}

void StratumTaskV1::protocolLoop()
{
    Board *board = SYSTEM_MODULE.getBoard();

    m_stratumAPI.resetUid();
    m_stratumAPI.clearBuffer();
    m_setupFailed = false;

    ///// Start Stratum Action
    // mining.subscribe - ID: 1
    bool success = m_stratumAPI.subscribe(m_transport, board->getMiningAgent(), board->getAsicModel());

    // mining.configure - ID: 2
    success = success && m_stratumAPI.configureVersionRolling(m_transport);

    // mining.authorize - ID: 3
    success = success && m_stratumAPI.authenticate(m_transport, m_config->getUser(), m_config->getPassword());

    // mining.suggest_difficulty - ID: 4
    success = success && m_stratumAPI.suggestDifficulty(m_transport, Config::getStratumDifficulty());

    // mining.mining.extranonce.subscribe - ID 5
    if (m_config->isEnonceSubscribeEnabled()) {
        success = success && m_stratumAPI.entranonceSubscribe(m_transport);
    }

    if (!success) {
        ESP_LOGE(m_tag, "Error sending Stratum setup commands!");
        return;
    }

    // All Stratum servers should send the first job with clear flag,
    // but we make sure to clear the jobs on the first job
    m_firstJob = true;

    char *line = nullptr;

    while (1) {
        if (!m_transport->isConnected()) {
            if (Config::isStratumKeepaliveEnabled()) {
                ESP_LOGW(m_tag, "Socket disconnected — possible TCP KeepAlive timeout (enabled)");
            } else {
                ESP_LOGW(m_tag, "Socket disconnected — no KeepAlive active");
            }
            break;
        }
        line = m_stratumAPI.receiveJsonRpcLine(m_transport);

        // release memory when out of scope
        MemoryGuard g(line);

        if (!line && !m_reconnect) {
            ESP_LOGE(m_tag, "Failed to receive JSON-RPC line, reconnecting ...");
            return;
        }

        if (m_reconnect) {
            ESP_LOGI(m_tag, "reconnect requested ...");
            return;
        }

        ESP_LOGI(m_tag, "rx: %s", line); // debug incoming stratum messages

        PSRAMAllocator allocator;
        JsonDocument doc(&allocator);

        // Deserialize JSON
        // we want to know if it's valid json before the connected callback is executed
        DeserializationError error = deserializeJson(doc, line);
        if (error) {
            ESP_LOGE(m_tag, "Unable to parse JSON: %s", error.c_str());
            return;
        }

        // we are pretty confident now that we have valid json and we can
        // call the connected callback
        if (!m_isConnected) {
            connectedCallback();
            m_isConnected = true;
        }

        // if stop is requested, don't dispatch anything
        // and break the loop
        if (m_stopFlag || POWER_MANAGEMENT_MODULE.isShutdown()) {
            return;
        }

        // parse the line
        m_manager->dispatch(m_index, doc);

        // without a valid subscribe result there is no extranonce to build jobs with;
        // treat it as a pool error (normal reconnect delay) instead of mining on
        if (m_setupFailed) {
            return;
        }
    }
}

void StratumTaskV1::submitShare(const char *jobid, const char *extranonce_2, const uint32_t ntime, const uint32_t nonce,
                              const uint32_t version_rolled, const uint32_t version_base)
{
    // V1 mining.submit expects version rolling bits (delta), not full version
    uint32_t version_delta = version_rolled ^ version_base;
    m_stratumAPI.submitShare(m_transport, m_config->getUser(), jobid, extranonce_2, ntime, nonce, version_delta);
}
