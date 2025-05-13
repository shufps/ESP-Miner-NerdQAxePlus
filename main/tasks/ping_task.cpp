#include "ping_task.h"
#include "esp_log.h"
#include "ping/ping_sock.h"
#include "lwip/inet.h"
#include "lwip/ip_addr.h"
#include "lwip/sockets.h"
#include "nvs_config.h"
#include "global_state.h"
#include <netdb.h>
#include <string>
#include <ctime>

// Default fallback values
#ifndef DELAY_SECONDS
#define DELAY_SECONDS 60             // Delay between pings in seconds
#endif

#ifndef PING_COUNTS
#define PING_COUNTS 5                // Number of pings to send
#endif

#ifndef DNS_CACHE_TTL_SECONDS
#define DNS_CACHE_TTL_SECONDS 3600   // TTL for cached DNS lookup
#endif

// Wrapper functions for later runtime config support
int get_ping_delay() {
    // Future: Replace with Config::getPingDelay();
    return DELAY_SECONDS;
}

int get_ping_count() {
    // Future: Replace with:
    // int value = Config::getPingCount();
    // int max_allowed = get_ping_delay() - 1;
    // return std::max(1, std::min(value, max_allowed));
    return PING_COUNTS;
}

static const char *TAG = "ping task";
static double last_ping_rtt_ms = 0.0;

struct PingResult {
    bool success;
    double avg_rtt_ms;
    const char* hostname;
    const char* label;
};

// Perform ping to a given hostname and return true if at least one reply received
PingResult perform_ping(const char *hostname, const char *label) {
    // Static variables to cache resolved hostname and IP address
    static std::string last_hostname;
    static ip_addr_t cached_target_addr;
    static bool has_cached_ip = false;
    static time_t last_dns_resolve_time = 0;

    ip4_addr_t ip4;
    PingResult result = { .success = false, .avg_rtt_ms = 0, .hostname = hostname, .label = label };

    // Determine if we need to refresh the DNS resolution
    time_t now = time(NULL);
    bool need_resolve = !has_cached_ip ||
                        last_hostname != hostname ||
                        difftime(now, last_dns_resolve_time) > DNS_CACHE_TTL_SECONDS;

    if (need_resolve) {
        struct addrinfo hints = {};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        struct addrinfo *res;
        int err = getaddrinfo(hostname, NULL, &hints, &res);
        if (err != 0 || res == NULL) {
            ESP_LOGE(TAG, "DNS lookup failed for host: %s (%s)", hostname, strerror(err));
            return result;
        }

        struct sockaddr_in *addr_in = (struct sockaddr_in *)res->ai_addr;
        ip4.addr = addr_in->sin_addr.s_addr;
        ip_addr_set_ip4_u32(&cached_target_addr, ip4.addr);
        cached_target_addr.type = IPADDR_TYPE_V4;

        freeaddrinfo(res);

        has_cached_ip = true;
        last_hostname = hostname;
        last_dns_resolve_time = now;

        ESP_LOGI(TAG, "Resolved hostname %s to IP: %s", hostname, inet_ntoa(addr_in->sin_addr));
    }

    // Configure the ping session
    esp_ping_config_t config = ESP_PING_DEFAULT_CONFIG();
    config.count = get_ping_count();
    config.interval_ms = 1000;
    config.timeout_ms = 1000;
    config.target_addr = cached_target_addr;

    struct {
        uint32_t replies = 0;
        uint32_t total_time = 0;
    } rtt_stats;

    // Set up ping callbacks
    esp_ping_callbacks_t cbs = {};
    cbs.cb_args = &rtt_stats;
    cbs.on_ping_success = [](esp_ping_handle_t hdl, void *args) {
        auto *stats = static_cast<decltype(rtt_stats)*>(args);
        uint32_t time_ms = 0;
        esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &time_ms, sizeof(time_ms));
        stats->total_time += time_ms;
        stats->replies++;
    };

    // Create and start the ping session
    esp_ping_handle_t ping;
    if (esp_ping_new_session(&config, &cbs, &ping) != ESP_OK) {
        return result;
    }

    if (esp_ping_start(ping) != ESP_OK) {
        esp_ping_delete_session(ping);
        return result;
    }

    // Wait until all ping requests have been sent
    while (true) {
        uint32_t sent = 0;
        esp_ping_get_profile(ping, ESP_PING_PROF_REQUEST, &sent, sizeof(sent));
        if (sent >= config.count) break;
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // Clean up ping session
    esp_ping_stop(ping);
    esp_ping_delete_session(ping);

    // Compute average RTT if at least one reply was received
    if (rtt_stats.replies > 0) {
        result.success = true;
        result.avg_rtt_ms = static_cast<double>(rtt_stats.total_time) / rtt_stats.replies;
        last_ping_rtt_ms = result.avg_rtt_ms;
    }

    return result;
}

// Task that continuously performs ping checks to the primary or fallback stratum server
void ping_task(void *pvParameters) {
    while (true) {
        bool useFallback = STRATUM_MANAGER.isUsingFallback();

        const char *hostname = useFallback
            ? Config::getStratumFallbackURL()
            : Config::getStratumURL();

        const char *label = useFallback ? "Fallback" : "Primary";

        PingResult result = perform_ping(hostname, label);

        if (result.success) {
            ESP_LOGI(TAG, "[%s]: Ping to %s succeeded, avg RTT: %.3f ms", result.label, result.hostname, result.avg_rtt_ms);
        } else {
            ESP_LOGW(TAG, "[%s]: Ping to %s failed", result.label, result.hostname);
        }

        vTaskDelay(pdMS_TO_TICKS(get_ping_delay() * 1000));
    }
}

// Getter for last recorded average RTT
double get_last_ping_rtt() {
    return last_ping_rtt_ms;
}
