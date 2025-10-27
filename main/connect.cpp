#include "connect.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "lwip/ip_addr.h"
#include "lwip/inet.h"

static const char *TAG = "wifi_apsta";

esp_err_t start_rest_server(void * pvParameters);

static EventGroupHandle_t s_evtgrp;
#define BIT_STA_GOT_IP    BIT0
#define BIT_STA_CONNECTED BIT1

static bool s_async_mode = false;
static esp_netif_t *s_sta_netif = NULL;

// Cache: configured AP SSID, and desired hostname (applied on start)
static char s_ap_ssid[33]{};   // 32+NUL
static char s_hostname[64]{};  // RFC 1123 label max 63

static bool s_ap_up = false;
static uint16_t s_retries = 0;
static const int kMaxImmediateRetries = 8;

static uint16_t s_notify_fail_after = 0;

static apsta_status_cb_t s_status_cb = NULL;
static void *s_status_cb_ctx = NULL;

static apsta_ap_state_cb_t s_ap_cb = NULL;
static void *s_ap_cb_ctx = NULL;

static inline void notify_status(apsta_wifi_status_t st) {
    if (s_status_cb) s_status_cb(st, s_retries, s_status_cb_ctx);
}

static inline void notify_ap_state(bool on) {
    if (s_ap_cb) s_ap_cb(on, s_ap_cb_ctx);
}

// --- Internal best-practice knobs ---
static void apply_best_practices(const apsta_config_t *cfg) {
    if (cfg->ps_disable) {
        // Disable power save while provisioning for faster association
        ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    }
    // Enable 11b/g/n on both interfaces
    ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N));
    ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_AP,  WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N));
}

static void evt_hdlr(void *arg, esp_event_base_t base, int32_t id, void *data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "STA start -> connect");
        notify_status(APSTA_WIFI_CONNECTING);
        esp_wifi_connect();

    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_CONNECTED) {
        ESP_LOGI(TAG, "STA connected (awaiting IP)");
        xEventGroupSetBits(s_evtgrp, BIT_STA_CONNECTED);

    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "STA got IP: " IPSTR, IP2STR(&e->ip_info.ip));
        s_retries = 0;
        xEventGroupSetBits(s_evtgrp, BIT_STA_GOT_IP);
        notify_status(APSTA_WIFI_CONNECTED);

        if (s_async_mode) {
            ESP_LOGI(TAG, "Dropping AP (switching to STA-only) [async]");
            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
            s_ap_up = false;
            notify_ap_state(false);
            // Optional: esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
        }

    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *disc = (wifi_event_sta_disconnected_t *)data;
        ESP_LOGW(TAG, "STA disconnected (reason=%d), retry=%u", disc->reason, s_retries);
        xEventGroupClearBits(s_evtgrp, BIT_STA_CONNECTED);
        xEventGroupClearBits(s_evtgrp, BIT_STA_GOT_IP);

        s_retries++;
        if (s_notify_fail_after > 0 && s_retries == s_notify_fail_after) {
            notify_status(APSTA_WIFI_CONNECT_FAILED);
        } else {
            notify_status(APSTA_WIFI_RETRYING);
        }

        if (s_retries <= kMaxImmediateRetries) {
            vTaskDelay(pdMS_TO_TICKS(500 + 250 * (s_retries - 1)));
        } else {
            vTaskDelay(pdMS_TO_TICKS(3000));
        }
        esp_wifi_connect();
    }
}


// Default to ETSI (DE) if unset
static void country_defaults_if_empty(wifi_country_t *c) {
    if (!c || c->cc[0]) return;
    c->cc[0] = 'D'; c->cc[1] = 'E'; c->cc[2] = '\0';
    c->schan = 1; c->nchan = 13; c->policy = WIFI_COUNTRY_POLICY_AUTO;
}

// Build "nerdaxe-xxxx" where xxxx is first two bytes of STA MAC
static esp_err_t build_temp_ap_ssid(char *buf, size_t len) {
    if (!buf || len < 13) return ESP_ERR_INVALID_ARG; // "nerdaxe-xxxx"=12+NUL
    uint8_t mac[6]{};
    esp_err_t err = esp_wifi_get_mac(WIFI_IF_STA, mac);
    if (err != ESP_OK) return err;
    int n = snprintf(buf, len, "nerdaxe-%02x%02x", mac[0], mac[1]);
    if (n < 0 || (size_t)n >= len) return ESP_ERR_INVALID_SIZE;
    return ESP_OK;
}

// Very small RFC 952/1123-ish sanitizer: [a-z0-9-], no leading/trailing '-', lowercase
static void sanitize_hostname(const char *in, char *out, size_t out_len) {
    if (!in || !out || out_len == 0) return;
    size_t j = 0;
    // convert and filter
    for (size_t i = 0; in[i] && j + 1 < out_len; ++i) {
        char c = (char)tolower((unsigned char)in[i]);
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-') {
            out[j++] = c;
        }
    }
    // trim leading/trailing '-'
    while (j > 0 && out[0] == '-') memmove(out, out + 1, --j);
    while (j > 0 && out[j - 1] == '-') j--;
    out[j] = '\0';
    if (j == 0) strlcpy(out, "esp", out_len); // fallback
}

static void fill_sta_cfg(wifi_config_t *sta, const apsta_config_t *cfg) {
    memset(sta, 0, sizeof(*sta));
    if (cfg->sta_ssid)  strlcpy((char *)sta->sta.ssid, cfg->sta_ssid, sizeof(sta->sta.ssid));
    if (cfg->sta_pass)  strlcpy((char *)sta->sta.password, cfg->sta_pass, sizeof(sta->sta.password));
    sta->sta.pmf_cfg.capable = true;
    sta->sta.pmf_cfg.required = false;
    sta->sta.scan_method = WIFI_FAST_SCAN;
    sta->sta.threshold.rssi = -80;
    sta->sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
}

static void fill_ap_cfg(wifi_config_t *ap, const apsta_config_t *cfg) {
    memset(ap, 0, sizeof(*ap));

    char tmp_ssid[33]{};
    const char *use_ssid = cfg->ap_ssid;

    if (!use_ssid || use_ssid[0] == '\0') {
        if (build_temp_ap_ssid(tmp_ssid, sizeof(tmp_ssid)) == ESP_OK) {
            use_ssid = tmp_ssid;
        } else {
            use_ssid = "nerdaxe-0000"; // fallback
        }
    }
    strlcpy((char *)ap->ap.ssid, use_ssid, sizeof(ap->ap.ssid));
    strlcpy(s_ap_ssid, use_ssid, sizeof(s_ap_ssid)); // cache for getter

    ap->ap.ssid_len = 0;
    ap->ap.channel = cfg->ap_channel ? cfg->ap_channel : 1;
    ap->ap.max_connection = cfg->ap_max_conn ? cfg->ap_max_conn : 4;

    // OPEN AP (no password)
    ap->ap.authmode = WIFI_AUTH_OPEN;
    ap->ap.password[0] = '\0';
    ap->ap.pmf_cfg.capable  = false;
    ap->ap.pmf_cfg.required = false;
    ap->ap.sae_pwe_h2e = WPA3_SAE_PWE_UNSPECIFIED; // 0
}

static esp_err_t ensure_basics_inited(void) {
    static bool inited = false;
    if (inited) return ESP_OK;

    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) return err;
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    s_sta_netif = esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t wic = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wic));

    s_evtgrp = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &evt_hdlr, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,   IP_EVENT_STA_GOT_IP, &evt_hdlr, NULL, NULL));

    inited = true;
    return ESP_OK;
}

static esp_err_t apply_hostname_if_available(void) {
    if (!s_sta_netif) return ESP_ERR_INVALID_STATE;
    if (s_hostname[0] == '\0') return ESP_OK; // nothing to apply
    // Must be set before DHCP client starts; we call this in start_common() before esp_wifi_start()
    esp_err_t err = esp_netif_set_hostname(s_sta_netif, s_hostname);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Hostname set to '%s'", s_hostname);
    } else {
        ESP_LOGW(TAG, "Failed to set hostname (%d)", err);
    }
    return err;
}

static esp_err_t start_common(const apsta_config_t *cfg) {
    if (!cfg) return ESP_ERR_INVALID_ARG;

    s_notify_fail_after = cfg->notify_fail_after_retries;

    wifi_country_t c = cfg->country;
    country_defaults_if_empty(&c);
    ESP_ERROR_CHECK(esp_wifi_set_country(&c));

    (void)apply_hostname_if_available();

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

    wifi_config_t sta_cfg; fill_sta_cfg(&sta_cfg, cfg);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));

    wifi_config_t ap_cfg;  fill_ap_cfg(&ap_cfg, cfg);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));

    apply_best_practices(cfg);

    ESP_ERROR_CHECK(esp_wifi_start());

    // Mark AP up after start in APSTA mode
    s_ap_up = true;
    notify_ap_state(true);

    ESP_LOGI(TAG, "AP+STA started; waiting for STA to get IP while AP is up...");
    return ESP_OK;
}

esp_err_t apsta_start_block_until_sta_ip_then_drop_ap(const apsta_config_t *cfg) {
    ESP_ERROR_CHECK(ensure_basics_inited());
    s_async_mode = false;
    ESP_ERROR_CHECK(start_common(cfg));

    start_rest_server(NULL);

    xEventGroupWaitBits(s_evtgrp, BIT_STA_GOT_IP, pdFALSE, pdTRUE, portMAX_DELAY);

    ESP_LOGI(TAG, "Dropping AP (switching to STA-only) [blocking]");
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    s_ap_up = false;
    notify_ap_state(false);
    return ESP_OK;
}

esp_err_t apsta_start_async_drop_ap_on_sta_ip(const apsta_config_t *cfg) {
    ESP_ERROR_CHECK(ensure_basics_inited());
    s_async_mode = true;
    return start_common(cfg);
}

// ---- Runtime helpers / Getters ----

esp_err_t apsta_set_country_by_code(const char cc_in[3]) {
    if (!cc_in || !cc_in[0] || !cc_in[1]) return ESP_ERR_INVALID_ARG;

    wifi_country_t c{};
    c.policy = WIFI_COUNTRY_POLICY_AUTO;
    c.schan = 1;

    if ((cc_in[0]=='D' && cc_in[1]=='E') || (cc_in[0]=='E' && cc_in[1]=='U')) {
        c.cc[0]='D'; c.cc[1]='E'; c.cc[2]='\0'; c.nchan = 13;
    } else if (cc_in[0]=='U' && cc_in[1]=='S') {
        c.cc[0]='U'; c.cc[1]='S'; c.cc[2]='\0'; c.nchan = 11;
    } else if (cc_in[0]=='J' && cc_in[1]=='P') {
        c.cc[0]='J'; c.cc[1]='P'; c.cc[2]='\0'; c.nchan = 13;
    } else {
        c.cc[0]=cc_in[0]; c.cc[1]=cc_in[1]; c.cc[2]='\0'; c.nchan = 13;
    }
    ESP_LOGI(TAG, "Setting country to %c%c (%d channels)", c.cc[0], c.cc[1], c.nchan);
    return esp_wifi_set_country(&c);
}

esp_err_t apsta_set_sta_credentials(const char *ssid, const char *pass) {
    if (!ssid) return ESP_ERR_INVALID_ARG;
    wifi_config_t sta_cfg{};
    strlcpy((char *)sta_cfg.sta.ssid, ssid, sizeof(sta_cfg.sta.ssid));
    if (pass) strlcpy((char *)sta_cfg.sta.password, pass, sizeof(sta_cfg.sta.password));
    sta_cfg.sta.pmf_cfg.capable = true;
    sta_cfg.sta.pmf_cfg.required = false;
    sta_cfg.sta.scan_method = WIFI_FAST_SCAN;
    sta_cfg.sta.threshold.rssi = -80;
    sta_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    return esp_wifi_set_config(WIFI_IF_STA, &sta_cfg);
}

bool apsta_sta_has_ip(void) {
    EventBits_t b = xEventGroupGetBits(s_evtgrp);
    return (b & BIT_STA_GOT_IP) != 0;
}

esp_err_t apsta_get_current_ap_ssid(char *buf, size_t buf_len) {
    if (!buf || buf_len == 0) return ESP_ERR_INVALID_ARG;
    if (s_ap_ssid[0] == '\0') return ESP_ERR_INVALID_STATE;
    strlcpy(buf, s_ap_ssid, buf_len);
    return ESP_OK;
}

esp_err_t apsta_make_temp_ap_ssid(char *buf, size_t buf_len) {
    return build_temp_ap_ssid(buf, buf_len);
}

esp_err_t apsta_get_sta_mac(uint8_t mac_out[6]) {
    if (!mac_out) return ESP_ERR_INVALID_ARG;
    return esp_wifi_get_mac(WIFI_IF_STA, mac_out);
}

esp_err_t apsta_get_sta_ip_str(char *buf, size_t buf_len) {
    if (!buf || buf_len < 8) return ESP_ERR_INVALID_ARG; // minimal "0.0.0.0"
    if (!s_sta_netif) return ESP_ERR_INVALID_STATE;

    esp_netif_ip_info_t ipi;
    if (esp_netif_get_ip_info(s_sta_netif, &ipi) != ESP_OK) {
        return ESP_ERR_INVALID_STATE;
    }
    if (ipi.ip.addr == 0) {
        return ESP_ERR_INVALID_STATE; // no IP yet
    }
    const char *s = ip4addr_ntoa((const ip4_addr_t *)&ipi.ip);
    if (!s) return ESP_ERR_INVALID_STATE;
    strlcpy(buf, s, buf_len);
    return ESP_OK;
}

// --- Hostname API ---

esp_err_t apsta_set_hostname(const char *hostname) {
    if (!hostname || !hostname[0]) return ESP_ERR_INVALID_ARG;
    sanitize_hostname(hostname, s_hostname, sizeof(s_hostname));
    if (s_sta_netif) {
        // If called before start_common(): great. If after, DHCP may already be running.
        // We still store it; it will be applied on next start. We do NOT bounce DHCP here.
        ESP_LOGI(TAG, "Hostname staged as '%s' (will apply before Wi-Fi start)", s_hostname);
    }
    return ESP_OK;
}

esp_err_t apsta_get_hostname(char *buf, size_t buf_len) {
    if (!buf || buf_len == 0) return ESP_ERR_INVALID_ARG;
    if (s_hostname[0]) {
        strlcpy(buf, s_hostname, buf_len);
        return ESP_OK;
    }
    // If not staged, try to read from netif (if already set)
    if (s_sta_netif) {
        const char *hn = NULL;
        if (esp_netif_get_hostname(s_sta_netif, &hn) == ESP_OK && hn && hn[0]) {
            strlcpy(buf, hn, buf_len);
            return ESP_OK;
        }
    }
    return ESP_ERR_INVALID_STATE;
}

bool apsta_is_ap_up(void) {
    return s_ap_up;
}

uint16_t apsta_get_retry_count(void) {
    return s_retries;
}

esp_err_t apsta_register_status_callback(apsta_status_cb_t cb, void *ctx) {
    s_status_cb = cb;
    s_status_cb_ctx = ctx;
    return ESP_OK;
}

esp_err_t apsta_register_ap_state_callback(apsta_ap_state_cb_t cb, void *ctx) {
    s_ap_cb = cb;
    s_ap_cb_ctx = ctx;
    return ESP_OK;
}
