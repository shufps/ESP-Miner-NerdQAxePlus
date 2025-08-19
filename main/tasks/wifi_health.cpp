#include "wifi_health.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lwip/stats.h"

static const char* TAG = "wifi_health";

// Helper: logs a single stats_proto block
static void log_stats_proto(const char* name, const stats_proto& s)
{
    ESP_LOGI(TAG, "%s: xmit=%" STAT_COUNTER_F
                  ", recv=%" STAT_COUNTER_F
                  ", drop=%" STAT_COUNTER_F
                  ", err=%" STAT_COUNTER_F
                  ", chkerr=%" STAT_COUNTER_F
                  ", lenerr=%" STAT_COUNTER_F
                  ", memerr=%" STAT_COUNTER_F,
             name,
             s.xmit,
             s.recv,
             s.drop,
             s.err,
             s.chkerr,
             s.lenerr,
             s.memerr);
}

static void reset_lwip_stats()
{
#if IP_STATS
    memset(&lwip_stats.ip, 0, sizeof(lwip_stats.ip));
#endif
#if TCP_STATS
    memset(&lwip_stats.tcp, 0, sizeof(lwip_stats.tcp));
#endif
#if UDP_STATS
    memset(&lwip_stats.udp, 0, sizeof(lwip_stats.udp));
#endif
#if ICMP_STATS
    memset(&lwip_stats.icmp, 0, sizeof(lwip_stats.icmp));
#endif
#if IPFRAG_STATS
    memset(&lwip_stats.ip_frag, 0, sizeof(lwip_stats.ip_frag));
#endif
#if ETHARP_STATS
    memset(&lwip_stats.etharp, 0, sizeof(lwip_stats.etharp));
#endif
#if LINK_STATS
    memset(&lwip_stats.link, 0, sizeof(lwip_stats.link));
#endif
}

void log_wifi_health()
{
    ESP_LOGI(TAG, "------ WiFi / lwIP Health ------");

#if IP_STATS
    log_stats_proto("IP", lwip_stats.ip);
#endif

#if TCP_STATS
    log_stats_proto("TCP", lwip_stats.tcp);
#endif

#if UDP_STATS
    log_stats_proto("UDP", lwip_stats.udp);
#endif

#if ICMP_STATS
    log_stats_proto("ICMP", lwip_stats.icmp);
#endif

#if IPFRAG_STATS
    log_stats_proto("IPFRAG", lwip_stats.ip_frag);
#endif

#if ETHARP_STATS
    log_stats_proto("ETHARP", lwip_stats.etharp);
#endif

#if LINK_STATS
    log_stats_proto("LINK", lwip_stats.link);
#endif

    // Optional: RSSI und SSID anzeigen
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        ESP_LOGI(TAG, "RSSI: %d dBm, SSID: %s, Channel: %d",
                 ap_info.rssi,
                 reinterpret_cast<const char*>(ap_info.ssid),
                 ap_info.primary);
    } else {
        ESP_LOGW(TAG, "WiFi not connected (RSSI/SSID unavailable)");
    }

    ESP_LOGI(TAG, "-------------------------------");
}

// Background task to periodically log health info
void wifi_monitor_task(void* arg)
{
    // Initial delay
    vTaskDelay(pdMS_TO_TICKS(300000));

    // reset stats
    reset_lwip_stats();

    while (true) {
        log_wifi_health();
        vTaskDelay(pdMS_TO_TICKS(300000));  // every 5 minutes
    }
}
