#include "ping_task.h"
#include "esp_log.h"
#include "ping/ping_sock.h"
#include "lwip/inet.h"
#include "lwip/ip_addr.h"
#include "lwip/sockets.h"
#include "nvs_config.h"

static const char *TAG = "ping task";

// Perform ping to a given hostname and return true if at least one reply received
bool perform_ping(const char *hostname) {
    ip_addr_t target_addr;
    ip4_addr_t ip4;

    if (!inet_aton(hostname, &ip4)) {
        ESP_LOGE(TAG, "Invalid IP address: %s", hostname);
        return false;
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
        ESP_LOGE(TAG, "Failed to create ping session");
        return false;
    }

    if (esp_ping_start(ping) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start ping session");
        esp_ping_delete_session(ping);
        return false;
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
        uint32_t avg_rtt = rtt_stats.total_time / rtt_stats.replies;
        ESP_LOGI(TAG, "Average RTT to %s: %lu ms", hostname, (unsigned long)avg_rtt);
        return true;
    } else {
        ESP_LOGW(TAG, "No reply from %s", hostname);
        return false;
    }
}

void ping_task(void *pvParameters) {
    while (true) {
        const char *primary = Config::getStratumURL();
        const char *fallback = Config::getStratumFallbackURL();

        if (!perform_ping(primary)) {
            perform_ping(fallback);
        }

        vTaskDelay(pdMS_TO_TICKS(60000)); // Once per minute
    }
}
