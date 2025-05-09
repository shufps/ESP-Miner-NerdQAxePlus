#include "ping_task.h"
#include "esp_log.h"
#include "ping/ping_sock.h"
#include "lwip/inet.h"
#include "lwip/ip_addr.h"
#include "lwip/sockets.h"
#include "nvs_config.h"
#include "global_state.h"

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
    ip_addr_t target_addr;
    ip4_addr_t ip4;

    PingResult result = { .success = false, .avg_rtt_ms = 0, .hostname = hostname, .label = label };

    if (!inet_aton(hostname, &ip4)) {
        ESP_LOGE(TAG, "Invalid IP address: %s", hostname);
        return result;
    }

    ip_addr_set_ip4_u32(&target_addr, ip4.addr);
    target_addr.type = IPADDR_TYPE_V4;

    esp_ping_config_t config = ESP_PING_DEFAULT_CONFIG();
    config.count = 4;
    config.interval_ms = 1000;
    config.timeout_ms = 1000;
    config.target_addr = target_addr;

    struct {
        uint32_t replies = 0;
        uint32_t total_time = 0;
    } rtt_stats;

    esp_ping_callbacks_t cbs = {};
    cbs.cb_args = &rtt_stats;
    cbs.on_ping_success = [](esp_ping_handle_t hdl, void *args) {
        auto *stats = static_cast<decltype(rtt_stats)*>(args);
        uint32_t time_ms = 0;
        esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &time_ms, sizeof(time_ms));
        stats->total_time += time_ms;
        stats->replies++;
    };

    esp_ping_handle_t ping;
    if (esp_ping_new_session(&config, &cbs, &ping) != ESP_OK) {
        return result;
    }

    if (esp_ping_start(ping) != ESP_OK) {
        esp_ping_delete_session(ping);
        return result;
    }

    while (true) {
        uint32_t sent = 0;
        esp_ping_get_profile(ping, ESP_PING_PROF_REQUEST, &sent, sizeof(sent));
        if (sent >= config.count) break;
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    esp_ping_stop(ping);
    esp_ping_delete_session(ping);

    if (rtt_stats.replies > 0) {
        result.success = true;
        result.avg_rtt_ms = static_cast<double>(rtt_stats.total_time) / rtt_stats.replies;
	last_ping_rtt_ms = result.avg_rtt_ms;
    }

    return result;
}

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

        vTaskDelay(pdMS_TO_TICKS(60000)); // Once per minute
    }
}

extern "C" double get_last_ping_rtt() {
    return last_ping_rtt_ms;
}
