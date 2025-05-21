#include "ping_task.h"
#include "esp_log.h"
#include "ping/ping_sock.h"
#include "lwip/inet.h"
#include "lwip/ip_addr.h"
#include "lwip/sockets.h"
#include "nvs_config.h"
#include "global_state.h"
#include "dns_task.h"
#include <inttypes.h> 

// Default fallback values
#ifndef DELAY_SECONDS
#define DELAY_SECONDS 60                    // Delay between pings in seconds
#endif

#ifndef PING_COUNTS
#define PING_COUNTS 5                       // Number of pings to send
#endif

#define RTT_STATS_OVERFLOW_THRESHOLD 0.9    // Reset from 90 % capacity

static const char *TAG = "ping task";
static double last_ping_rtt_ms = 0.0;

// Global RTT stats structure
static struct {
    uint16_t replies = 0;                   // Total number of successful ping replies
    uint32_t total_time = 0;                // Total accumulated round-trip time in ms
} rtt_stats;

// Reset RTT stats explicitly (e.g., on hostname change - or for the one in a billion edge case. Literally.)
void reset_rtt_stats() {
    rtt_stats.replies = 0;
    rtt_stats.total_time = 0;
}

// Overflow check: just in case uptime is > 490 days with current defines
void validate_rtt_stats() {
    constexpr uint16_t MAX_UINT16 = UINT16_MAX;
    const uint16_t replies_limit = (uint16_t)(MAX_UINT16 * RTT_STATS_OVERFLOW_THRESHOLD); // e.g. ~58981 for 90%

    if (rtt_stats.replies >= replies_limit) {
        ESP_LOGW(TAG, "Resetting RTT stats due to replies overflow risk (%" PRIu16 ")", rtt_stats.replies);
        reset_rtt_stats();
    }
}

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

struct PingResult {
    bool success;
    double avg_rtt_ms;
    const char* hostname;
    const char* label;
};

// Perform ping to a given hostname and return true if at least one reply received
PingResult perform_ping(const char *hostname, const char *label) {
    PingResult result = { .success = false, .avg_rtt_ms = 0, .hostname = hostname, .label = label };

    ip_addr_t target_addr;
    if (!resolve_hostname(hostname, &target_addr)) {
        ESP_LOGE(TAG, "Hostname resolution failed.");
        return result;
    }

    // Configure the ping session
    esp_ping_config_t config = ESP_PING_DEFAULT_CONFIG();
    config.count = get_ping_count();
    config.interval_ms = 1000;
    config.timeout_ms = 1000;
    config.target_addr = target_addr;


    // Set up ping callbacks - static to reduce repeated stack allocations
    static esp_ping_callbacks_t cbs = {};
    cbs.cb_args = &rtt_stats;
    cbs.on_ping_success = [](esp_ping_handle_t hdl, void *args) {
        auto *stats = static_cast<decltype(&rtt_stats)>(args);
        uint32_t time_ms = 0;
        if (esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &time_ms, sizeof(time_ms)) == ESP_OK) {
            stats->total_time += time_ms;
            stats->replies++;
        } else {
            ESP_LOGW(TAG, "Failed to retrieve RTT statistics from ping profile.");
        }
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
    const char* last_hostname = nullptr;

    while (true) {
        if (!STRATUM_MANAGER.isAnyConnected()) {
            ESP_LOGW(TAG, "Stratum not connected yet. Skipping ping...");
            vTaskDelay(pdMS_TO_TICKS(10000)); // 10s retry delay
            continue;
        }

        bool useFallback = STRATUM_MANAGER.isUsingFallback();

        const char *current_hostname = useFallback
            ? Config::getStratumFallbackURL()
            : Config::getStratumURL();

        const char *label = useFallback ? "Fallback" : "Primary";

        // Check if hostname has changed
        bool hostname_changed = (!last_hostname || strcmp(last_hostname, current_hostname) != 0);

        if (!hostname_changed) {
            // regular delay if hostname has NOT changed
            vTaskDelay(pdMS_TO_TICKS(get_ping_delay() * 1000));
        } else {
            // reset stats if hostname changes
            reset_rtt_stats();
        }

        validate_rtt_stats();

        // Perform ping
        PingResult result = perform_ping(current_hostname, label);

        if (result.success) {
            ESP_LOGI(TAG, "[%s]: Ping to %s succeeded, avg RTT: %.2f ms", result.label, result.hostname, result.avg_rtt_ms);
        } else {
            ESP_LOGW(TAG, "[%s]: Ping to %s failed", result.label, result.hostname);
        }

        last_hostname = current_hostname;
    }
}

// Getter for last recorded average RTT
double get_last_ping_rtt() {
    return last_ping_rtt_ms;
}
