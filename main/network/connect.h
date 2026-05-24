#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_netif.h"
#include "lwip/sys.h"
#include <arpa/inet.h>
#include <lwip/netdb.h>

/* Bits for the WiFi event group */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

typedef enum
{
    WIFI_CONNECTED,
    WIFI_DISCONNECTED,
    WIFI_CONNECTING,
    WIFI_DISCONNECTING,
    WIFI_CONNECT_FAILED,
    WIFI_RETRYING,
} wifi_status_t;

/* Simple hook signature (no std::function etc.) */
typedef void (*WifiHookFn)();

void wifi_softap_on(void);
void wifi_softap_off(void);

/* Now returns the STA netif (useful for routing decisions) */
esp_netif_t *wifi_init(const char *wifi_ssid, const char *wifi_pass, const char *hostname);

EventBits_t wifi_connect(void);
EventBits_t wifi_wait_connected_ms(TickType_t ticks);

void generate_ssid(char *ssid);

bool connect_get_ip_addr(char *buf, size_t buf_len);
const char *connect_get_mac_addr(void);

/* Netif getters */
esp_netif_t *wifi_get_sta_netif(void);
esp_netif_t *wifi_get_ap_netif(void);

/* Hooks for the NetworkManager */
void wifi_set_hook_got_ip(WifiHookFn fn);
void wifi_set_hook_disconnected(WifiHookFn fn);
