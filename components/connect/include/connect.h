#pragma once

#ifdef __cplusplus
extern "C" {
#endif


#include "lwip/sys.h"
#include <arpa/inet.h>
#include <lwip/netdb.h>
#include <stdint.h>

#include "freertos/event_groups.h"
#include "esp_err.h"
#include "esp_wifi_types.h"

/* Maximum number of access points returned by wifi_scan() */
#define WIFI_SCAN_MAX_APS 20

/* Compact record returned by wifi_scan() */
typedef struct {
    char ssid[33];              // 32 chars + null terminator
    int8_t rssi;                // Signal strength in dBm
    wifi_auth_mode_t authmode;  // WiFi security type
} wifi_ap_record_simple_t;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

// enum of wifi statuses
typedef enum
{
    WIFI_CONNECTED,
    WIFI_DISCONNECTED,
    WIFI_CONNECTING,
    WIFI_DISCONNECTING,
    WIFI_CONNECT_FAILED,
    WIFI_RETRYING,
} wifi_status_t;

void wifi_softap_on(void);
void wifi_softap_off(void);
void wifi_init(const char *wifi_ssid, const char *wifi_pass, const char *hostname);
EventBits_t wifi_connect(void);
void generate_ssid(char *ssid);
bool connect_get_ip_addr(char *buf, size_t buf_len);
const char* connect_get_mac_addr();
EventBits_t wifi_wait_connected_ms(TickType_t ticks);

/* Scan for available WiFi networks. Blocks until scan completes (up to ~10s).
 * Fills ap_records (capacity WIFI_SCAN_MAX_APS) and sets *ap_count.
 * Returns ESP_OK on success. */
esp_err_t wifi_scan(wifi_ap_record_simple_t *ap_records, uint16_t *ap_count);

#ifdef __cplusplus
}
#endif

