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
#define PING_COUNT 7           // number of pings per round
#define PING_INTERVAL_MS 1000  // delay between individual pings in ms
#define PING_TIMEOUT_MS 1000   // timeout per ping request in ms
#define PING_DELAY 60          // delay between ping rounds in seconds
#define HISTORY_WINDOW_SEC 900 // total window to calculate recent packet loss (900s = 15min)
#define HISTORY_SIZE (HISTORY_WINDOW_SEC / PING_DELAY)  // number of historical entries based on ping interval

static const char *TAG = "ping task";
static double last_ping_rtt_ms = 0.0;

// Result structure used by ping logic
struct PingResult {
    bool success;
    uint16_t replies;
    double avg_rtt_ms;
    double min_rtt_ms;
    double max_rtt_ms;
};

struct PingHistory {
    uint16_t sent;
    uint16_t received;
};

static PingHistory ping_history[HISTORY_SIZE];  // buffer storing ping results
static int history_index = 0;
static int history_count = 0;

static void record_ping_result(uint16_t sent, uint16_t received) {
    ping_history[history_index].sent = sent;
    ping_history[history_index].received = received;

    history_index = (history_index + 1) % HISTORY_SIZE;
    if (history_count < HISTORY_SIZE) {
        history_count++;
    }
}

static double get_recent_packet_loss() {
    uint32_t total_sent = 0;
    uint32_t total_recv = 0;

    for (int i = 0; i < history_count; i++) {
        total_sent += ping_history[i].sent;
        total_recv += ping_history[i].received;
    }

    if (total_sent == 0) return 0.0;
    return (total_sent - total_recv) / (double)total_sent;
}

// Run a ping session using resolved IP from STRATUM_MANAGER
PingResult perform_ping(const char* ip_str, const char* hostname_str) {
    PingResult result = { .success = false, .replies = 0, .avg_rtt_ms = 0.0, .min_rtt_ms = 1e6, .max_rtt_ms = 0.0 };

    ip_addr_t target_addr;
    if (!ip_str || !ipaddr_aton(ip_str, &target_addr)) {
        ESP_LOGE(TAG, "Invalid IP string passed to perform_ping (%s)", ip_str ? ip_str : "null");
        return result;
    }

    struct PingStats {
        uint16_t replies;
        uint32_t total_time_ms;
        double min_rtt;
        double max_rtt;
        const char* hostname;
        bool header_shown;
    } stats = { .min_rtt = 1e6, .max_rtt = 0.0, .hostname = hostname_str, .header_shown = false };

    // Configure ping session
    esp_ping_config_t config = ESP_PING_DEFAULT_CONFIG();
    config.count = PING_COUNT;
    config.interval_ms = PING_INTERVAL_MS;
    config.timeout_ms = PING_TIMEOUT_MS;
    config.target_addr = target_addr;

    // Set callback to accumulate successful replies
    esp_ping_callbacks_t cbs = {};
    cbs.cb_args = &stats;
    cbs.on_ping_success = [](esp_ping_handle_t hdl, void *args) {
        PingStats* s = (PingStats*) args;
        uint32_t time_ms = 0, seq = 0, ttl = 0, size = 0;
        ip_addr_t addr;

        bool ok_time = esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &time_ms, sizeof(time_ms)) == ESP_OK;
        bool ok_seq  = esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO,  &seq, sizeof(seq)) == ESP_OK;
        bool ok_ttl  = esp_ping_get_profile(hdl, ESP_PING_PROF_TTL,    &ttl, sizeof(ttl)) == ESP_OK;
        bool ok_size = esp_ping_get_profile(hdl, ESP_PING_PROF_SIZE,   &size, sizeof(size)) == ESP_OK;
        bool ok_ip   = esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &addr, sizeof(addr)) == ESP_OK;

        if (ok_time) {
            const char* ip_str = ok_ip ? ipaddr_ntoa(&addr) : "?.?.?.?";
            if (!s->header_shown && ok_ip && ok_size) {
                ESP_LOGI(TAG, "PING %s (%s): %lu data bytes", s->hostname, ip_str, (unsigned long)size);
                s->header_shown = true;
            }

            s->total_time_ms += time_ms;
            s->replies++;
            if (time_ms < s->min_rtt) s->min_rtt = time_ms;
            if (time_ms > s->max_rtt) s->max_rtt = time_ms;

            uint32_t icmp_seq = ok_seq ? seq : s->replies;
            uint32_t pkt_size = ok_size ? size : 64;

            ESP_LOGI(TAG, "%lu bytes from %s: icmp_seq=%lu ttl=%lu time=%lu ms",
                     (unsigned long)pkt_size, ip_str,
                     (unsigned long)icmp_seq,
                     (unsigned long)(ok_ttl ? ttl : 64),
                     (unsigned long)time_ms);
        }
    };

    // Configure and start ping session
    esp_ping_handle_t ping = NULL;
    if (esp_ping_new_session(&config, &cbs, &ping) != ESP_OK) {
        ESP_LOGE(TAG, "Error creating ping session");
        if (ping) esp_ping_delete_session(ping);  // Ensure memory is freed in case of prior allocation
        return result;
    }

    if (esp_ping_start(ping) != ESP_OK) {
        ESP_LOGE(TAG, "Error starting ping session");
        esp_ping_delete_session(ping); // Free memory if start fails
        return result;
    }

    // Wait until all replies are received or timeout expires
    const int max_wait_ms = PING_COUNT * (PING_TIMEOUT_MS + 100);
    int wait = 0;
    uint32_t replies = 0, sent = 0;

    while (wait < max_wait_ms) {
        esp_ping_get_profile(ping, ESP_PING_PROF_REPLY, &replies, sizeof(replies));
        esp_ping_get_profile(ping, ESP_PING_PROF_REQUEST, &sent, sizeof(sent));

        if (replies >= config.count) {
            vTaskDelay(pdMS_TO_TICKS(50)); // allow final callback to settle
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
        wait += 100;
    }

    // Stop session
    esp_ping_stop(ping);

    // Final verification: check if any replies were missing
    esp_ping_get_profile(ping, ESP_PING_PROF_REPLY, &replies, sizeof(replies));
    esp_ping_get_profile(ping, ESP_PING_PROF_REQUEST, &sent, sizeof(sent));

    // Clean up session
    esp_ping_delete_session(ping);

    // Store results if valid replies exist
    if (stats.replies > 0) {
        double round_avg = (double) stats.total_time_ms / stats.replies;
        result.success = true;
        result.replies = stats.replies;
        result.avg_rtt_ms = round_avg;
        result.min_rtt_ms = stats.min_rtt;
        result.max_rtt_ms = stats.max_rtt;
        last_ping_rtt_ms = result.avg_rtt_ms;
    }

    return result;
}

// Periodic task that pings stratum target every PING_DELAY using resolved IP
void ping_task(void *pvParameters) {
    const StratumConfig* last_config = nullptr;

    while (true) {
        if (!STRATUM_MANAGER.isAnyConnected()) {
            ESP_LOGW(TAG, "Stratum not connected. Skipping ping...");
            vTaskDelay(pdMS_TO_TICKS(10000));
            continue;
        }

        int index = STRATUM_MANAGER.isUsingFallback() ? 1 : 0;

        const StratumConfig* current_config = SYSTEM_MODULE.getStratumConfig(index);
        const char* hostname = current_config ? current_config->host : nullptr;
        const char* ip_str = STRATUM_MANAGER.getResolvedIpForSelected();

        if (!hostname || !ip_str) {
            ESP_LOGE(TAG, "No resolved IP for current hostname");
            vTaskDelay(pdMS_TO_TICKS(PING_DELAY * 1000));
            continue;
        }

        if (last_config != current_config) {
            last_ping_rtt_ms = 0.0;
        }

        PingResult result = perform_ping(ip_str, hostname);

        ESP_LOGI(TAG, "--- %s ping statistics ---", hostname);
        double loss_current = 100.0 * (PING_COUNT - result.replies) / (double)PING_COUNT;
        ESP_LOGI(TAG, "%u packets transmitted, %u packets received, %.1f%% packet loss",
                PING_COUNT, result.replies, loss_current);
        if (result.success) {
            ESP_LOGI(TAG, "round-trip min/avg/max = %.2f/%.2f/%.2f ms",
                     result.min_rtt_ms, result.avg_rtt_ms, result.max_rtt_ms);
        }
        record_ping_result(PING_COUNT, result.replies);
        ESP_LOGI(TAG, "Recent %d-min packet loss: %.1f%%", HISTORY_WINDOW_SEC / 60, 100.0 * get_recent_packet_loss());
        last_config = current_config;
        vTaskDelay(pdMS_TO_TICKS(PING_DELAY * 1000));
    }
}

// Provide latest average RTT to other modules
double get_last_ping_rtt() {
    return last_ping_rtt_ms;
}

// Provide recent few minutes packet loss to other modules
double get_recent_ping_loss() {
    return get_recent_packet_loss();
}