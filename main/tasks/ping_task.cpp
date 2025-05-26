#include "ping_task.h"
#include "esp_log.h"
#include "ping/ping_sock.h"
#include "lwip/inet.h"
#include "lwip/ip_addr.h"
#include "lwip/sockets.h"
#include "nvs_config.h"
#include "global_state.h"
#include "stratum_task.h"

// Configuration for ping intervals and history
#define PING_COUNT 5           // number of pings per round
#define PING_INTERVAL_MS 1000  // delay between individual pings in ms
#define PING_TIMEOUT_MS 1000   // timeout per ping request in ms
#define PING_DELAY 30          // delay between ping rounds in seconds
#define RTT_HISTORY_LENGTH 11  // number of RTT averages to retain in history

static const char *TAG = "ping task";
static double last_ping_rtt_ms = 0.0;

// Circular buffer to store last RTT averages
static struct {
    uint16_t count = 0;
    uint16_t index = 0;
    double samples[RTT_HISTORY_LENGTH] = {0};
} rtt_stats;

// Clear all RTT samples
void reset_rtt_stats() {
    rtt_stats.count = 0;
    rtt_stats.index = 0;
    for (int i = 0; i < RTT_HISTORY_LENGTH; ++i) {
        rtt_stats.samples[i] = 0.0;
    }
}

// Add a new RTT sample to the circular buffer
void add_rtt_sample(double rtt_ms) {
    rtt_stats.samples[rtt_stats.index] = rtt_ms;
    rtt_stats.index = (rtt_stats.index + 1) % RTT_HISTORY_LENGTH;
    if (rtt_stats.count < RTT_HISTORY_LENGTH) {
        rtt_stats.count++;
    }
}

// get average RTT
double get_average_rtt() {
    if (rtt_stats.count == 0) return 0.0;
    double sum = 0.0;
    for (int i = 0; i < rtt_stats.count; ++i) {
        sum += rtt_stats.samples[i];
    }
    return sum / rtt_stats.count;
}

// get minimum RTT
double get_min_rtt() {
    if (rtt_stats.count == 0) return 0.0;
    double min = rtt_stats.samples[0];
    for (int i = 1; i < rtt_stats.count; ++i) {
        if (rtt_stats.samples[i] < min) min = rtt_stats.samples[i];
    }
    return min;
}

// get maximum RTT
double get_max_rtt() {
    if (rtt_stats.count == 0) return 0.0;
    double max = rtt_stats.samples[0];
    for (int i = 1; i < rtt_stats.count; ++i) {
        if (rtt_stats.samples[i] > max) max = rtt_stats.samples[i];
    }
    return max;
}

// Result structure used by ping logic
struct PingResult {
    bool success;
    double avg_rtt_ms;
    const char* hostname;
    const char* label;
};

// Run a ping session using resolved IP from STRATUM_MANAGER
PingResult perform_ping(const char *hostname, const char *label) {
    PingResult result = { .success = false, .avg_rtt_ms = 0.0, .hostname = hostname, .label = label };

    // get IP from stratum_task
    const char* ip_str = STRATUM_MANAGER.getResolvedIpForSelected();
    ip_addr_t target_addr;

    if (!ip_str || !ipaddr_aton(ip_str, &target_addr)) {
        ESP_LOGE(TAG, "No valid IP from StratumManager (%s)", ip_str ? ip_str : "null");
        return result;
    }

    struct PingStats {
        uint16_t replies;
        uint32_t total_time_ms;
    } stats = {};

    // Configure ping session
    esp_ping_config_t config = ESP_PING_DEFAULT_CONFIG();
    config.count = PING_COUNT;
    config.interval_ms = PING_INTERVAL_MS;
    config.timeout_ms = PING_TIMEOUT_MS;
    config.target_addr = target_addr;

    // Set callback to accumulate successful replies
    static esp_ping_callbacks_t cbs = {};
    cbs.cb_args = &stats;
    cbs.on_ping_success = [](esp_ping_handle_t hdl, void *args) {
        PingStats* s = (PingStats*) args;
        uint32_t time_ms = 0;
        if (esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &time_ms, sizeof(time_ms)) == ESP_OK) {
            s->total_time_ms += time_ms;
            s->replies++;
        }
    };

    // Start ping session
    esp_ping_handle_t ping;
    if (esp_ping_new_session(&config, &cbs, &ping) != ESP_OK) return result;
    if (esp_ping_start(ping) != ESP_OK) {
        esp_ping_delete_session(ping);
        return result;
    }

    // Wait until all pings are sent
    while (true) {
        uint32_t sent = 0;
        esp_ping_get_profile(ping, ESP_PING_PROF_REQUEST, &sent, sizeof(sent));
        if (sent >= config.count) break;
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    esp_ping_stop(ping);
    esp_ping_delete_session(ping);

    // Store average of this round if valid replies exist
    if (stats.replies > 0) {
        double round_avg = (double) stats.total_time_ms / stats.replies;
        add_rtt_sample(round_avg);
        result.success = true;
        result.avg_rtt_ms = get_average_rtt();
        last_ping_rtt_ms = result.avg_rtt_ms;
    }

    return result;
}

// Periodic task that pings stratum target every 20s using resolved IP
void ping_task(void *pvParameters) {
    const char* last_hostname = nullptr;

    while (true) {
        if (!STRATUM_MANAGER.isAnyConnected()) {
            ESP_LOGW(TAG, "Stratum not connected. Skipping ping...");
            vTaskDelay(pdMS_TO_TICKS(10000));
            continue;
        }

        bool useFallback = STRATUM_MANAGER.isUsingFallback();
        const char *current_hostname = useFallback
            ? Config::getStratumFallbackURL()
            : Config::getStratumURL();
        const char *label = useFallback ? "Fallback" : "Primary";

        bool hostname_changed = (!last_hostname || strcmp(last_hostname, current_hostname) != 0);
        if (hostname_changed) {
            reset_rtt_stats();
        }

        PingResult result = perform_ping(current_hostname, label);

        if (result.success) {
            ESP_LOGI(TAG, "[%s]: Ping to %s succeeded, RTT min/avg/max = %.2f/%.2f/%.2f ms",
                     result.label, result.hostname, get_min_rtt(), result.avg_rtt_ms, get_max_rtt());
        } else {
            ESP_LOGW(TAG, "[%s]: Ping to %s failed", result.label, result.hostname);
        }

        last_hostname = current_hostname;
        vTaskDelay(pdMS_TO_TICKS(PING_DELAY * 1000));
    }
}

// Provide latest average RTT to other modules
double get_last_ping_rtt() {
    return last_ping_rtt_ms;
}
