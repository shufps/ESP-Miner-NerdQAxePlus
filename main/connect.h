#pragma once
#include "esp_err.h"
#include "esp_wifi.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef enum {
    APSTA_OK = 0,
    APSTA_ERR_INVALID_ARG = 0x10000,
} apsta_err_t;

typedef struct {
    const char *sta_ssid;
    const char *sta_pass;
    const char *ap_ssid;          // if NULL/empty -> auto "nerdaxe-xxxx"
    uint8_t     ap_channel;
    uint8_t     ap_max_conn;
    bool        ps_disable;       // disable PS during provisioning
    wifi_country_t country;       // if .cc[0]==0 -> defaults to DE/ETSI

    uint16_t    notify_fail_after_retries;
} apsta_config_t;

// Start modes
esp_err_t apsta_start_block_until_sta_ip_then_drop_ap(const apsta_config_t *cfg);
esp_err_t apsta_start_async_drop_ap_on_sta_ip(const apsta_config_t *cfg);

// Web-UI helpers
esp_err_t apsta_set_country_by_code(const char cc[3]);
esp_err_t apsta_set_sta_credentials(const char *ssid, const char *pass);
bool      apsta_sta_has_ip(void);

// Getters
esp_err_t apsta_get_current_ap_ssid(char *buf, size_t buf_len);
esp_err_t apsta_make_temp_ap_ssid(char *buf, size_t buf_len);
esp_err_t apsta_get_sta_mac(uint8_t mac_out[6]);
esp_err_t apsta_get_sta_ip_str(char *buf, size_t buf_len);

// Hostname (STA)
esp_err_t apsta_set_hostname(const char *hostname);
esp_err_t apsta_get_hostname(char *buf, size_t buf_len);

// NEW: AP state + retry/status interface
bool      apsta_is_ap_up(void);
uint16_t  apsta_get_retry_count(void);

// Status callback API (maps 1:1 zu deinem Beispiel)
typedef enum {
    APSTA_WIFI_CONNECTING,
    APSTA_WIFI_RETRYING,
    APSTA_WIFI_CONNECTED,
    APSTA_WIFI_CONNECT_FAILED,
    APSTA_WIFI_DISCONNECTED,
} apsta_wifi_status_t;

typedef void (*apsta_status_cb_t)(apsta_wifi_status_t status, uint16_t retry_count, void *user_ctx);
esp_err_t apsta_register_status_callback(apsta_status_cb_t cb, void *user_ctx);

typedef void (*apsta_ap_state_cb_t)(bool ap_on, void *user_ctx);
esp_err_t apsta_register_ap_state_callback(apsta_ap_state_cb_t cb, void *user_ctx);

