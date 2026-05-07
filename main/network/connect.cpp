#include "connect.h"

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "lwip/err.h"

/* Implemented in main.cpp (C++ now, so linkage matches) */
void MINER_set_wifi_status(wifi_status_t status, uint16_t retry_count);
void MINER_set_ap_status(bool state);

static const char *TAG = "wifi";

#define WIFI_MAXIMUM_RETRY 5

static EventGroupHandle_t s_wifi_event_group = nullptr;

static int s_retry_num = 0;

static char s_ip_addr[20] = "0.0.0.0";
static char s_mac_addr[18] = {0};
static bool s_ip_valid = false;

static esp_netif_t *s_netif_ap = nullptr;
static esp_netif_t *s_netif_sta = nullptr;

static bool s_inited = false;

/* Optional hooks used by NetworkManager */
static WifiHookFn s_hook_got_ip = nullptr;
static WifiHookFn s_hook_disconnected = nullptr;

bool connect_get_ip_addr(char *buf, size_t buf_len)
{
    if (!buf || buf_len == 0) {
        return s_ip_valid;
    }
    /* Always NUL-terminate */
    strncpy(buf, s_ip_addr, buf_len);
    buf[buf_len - 1] = '\0';
    return s_ip_valid;
}

const char *connect_get_mac_addr(void)
{
    return s_mac_addr;
}

esp_netif_t *wifi_get_sta_netif(void)
{
    return s_netif_sta;
}
esp_netif_t *wifi_get_ap_netif(void)
{
    return s_netif_ap;
}

void wifi_set_hook_got_ip(WifiHookFn fn)
{
    s_hook_got_ip = fn;
}
void wifi_set_hook_disconnected(WifiHookFn fn)
{
    s_hook_disconnected = fn;
}

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void) arg;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        /* Clear connected state */
        s_ip_valid = false;
        strncpy(s_ip_addr, "0.0.0.0", sizeof(s_ip_addr));
        s_ip_addr[sizeof(s_ip_addr) - 1] = '\0';

        if (s_wifi_event_group) {
            xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        }

        if (s_hook_disconnected) {
            s_hook_disconnected();
        }

        /* Small backoff */
        vTaskDelay(pdMS_TO_TICKS(1000));

        if (s_retry_num < WIFI_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retrying WiFi connection...");
            MINER_set_wifi_status(WIFI_RETRYING, s_retry_num);
        } else {
            /* Signal initial failure so wifi_connect() can return */
            if (s_wifi_event_group) {
                xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            }
            ESP_LOGW(TAG, "Could not connect to WiFi (initial).");
            MINER_set_wifi_status(WIFI_CONNECT_FAILED, 0);

            /* Keep reconnecting indefinitely in background */
            s_retry_num = 0;
            esp_wifi_connect();
        }
        return;
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;

        snprintf(s_ip_addr, sizeof(s_ip_addr), IPSTR, IP2STR(&event->ip_info.ip));
        s_ip_valid = true;

        ESP_LOGI(TAG, "WiFi got IP: %s", s_ip_addr);

        s_retry_num = 0;

        if (s_wifi_event_group) {
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        }

        MINER_set_wifi_status(WIFI_CONNECTED, 0);

        if (s_hook_got_ip) {
            s_hook_got_ip();
        }
        return;
    }
}

EventBits_t wifi_wait_connected_ms(TickType_t ticks)
{
    if (!s_wifi_event_group) {
        return 0;
    }
    return xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, ticks);
}

static void generate_ssid_impl(char *ssid)
{
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_AP, mac);
    snprintf(ssid, 32, "Nerdaxe_%02X%02X", mac[4], mac[5]);
}

void generate_ssid(char *ssid)
{
    generate_ssid_impl(ssid);
}

static esp_netif_t *wifi_init_softap(void)
{
    esp_netif_t *esp_netif_ap = esp_netif_create_default_wifi_ap();

    char ssid_with_mac[13] = {0};
    generate_ssid_impl(ssid_with_mac);

    wifi_config_t wifi_ap_config;
    memset(&wifi_ap_config, 0, sizeof(wifi_ap_config));

    strncpy((char *) wifi_ap_config.ap.ssid, ssid_with_mac, sizeof(wifi_ap_config.ap.ssid));
    wifi_ap_config.ap.ssid_len = strlen(ssid_with_mac);
    wifi_ap_config.ap.channel = 1;
    wifi_ap_config.ap.max_connection = 30;
    wifi_ap_config.ap.authmode = WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config));

    return esp_netif_ap;
}

void wifi_softap_off(void)
{
    ESP_LOGI(TAG, "WiFi AP off (STA only)");
    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK && err != ESP_ERR_WIFI_STOP_STATE) {
        ESP_ERROR_CHECK(err);
    }
    MINER_set_ap_status(false);
}

void wifi_softap_on(void)
{
    ESP_LOGI(TAG, "WiFi AP on (APSTA)");
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    MINER_set_ap_status(true);
}

static esp_netif_t *wifi_init_sta(const char *wifi_ssid, const char *wifi_pass)
{
    esp_netif_t *esp_netif_sta = esp_netif_create_default_wifi_sta();

    wifi_config_t wifi_sta_config;
    memset(&wifi_sta_config, 0, sizeof(wifi_sta_config));

    /* Keep it simple: scan & connect parameters */
    wifi_sta_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    wifi_sta_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    wifi_sta_config.sta.pmf_cfg.capable = true;
    wifi_sta_config.sta.pmf_cfg.required = false;
    wifi_sta_config.sta.btm_enabled = 1;
    wifi_sta_config.sta.rm_enabled = 1;

    /* Copy credentials */
    strncpy((char *) wifi_sta_config.sta.ssid, wifi_ssid, sizeof(wifi_sta_config.sta.ssid));
    wifi_sta_config.sta.ssid[sizeof(wifi_sta_config.sta.ssid) - 1] = '\0';

    strncpy((char *) wifi_sta_config.sta.password, wifi_pass, sizeof(wifi_sta_config.sta.password));
    wifi_sta_config.sta.password[sizeof(wifi_sta_config.sta.password) - 1] = '\0';

    /* Allow open networks if password empty */
    if (wifi_sta_config.sta.password[0] == '\0') {
        wifi_sta_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
        ESP_LOGI(TAG, "WiFi password empty -> allow open networks");
    } else {
        wifi_sta_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config));
    return esp_netif_sta;
}

esp_netif_t *wifi_init(const char *wifi_ssid, const char *wifi_pass, const char *hostname)
{
    if (!s_wifi_event_group) {
        s_wifi_event_group = xEventGroupCreate();
    }

    strncpy(s_ip_addr, "0.0.0.0", sizeof(s_ip_addr));
    s_ip_addr[sizeof(s_ip_addr) - 1] = '\0';
    s_ip_valid = false;

    if (!s_inited) {
        esp_event_handler_instance_t instance_any_id;
        esp_event_handler_instance_t instance_got_ip;

        ESP_ERROR_CHECK(
            esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, nullptr, &instance_any_id));

        ESP_ERROR_CHECK(
            esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, nullptr, &instance_got_ip));

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

        esp_err_t err = esp_wifi_init(&cfg);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_ERROR_CHECK(err);
        }

        s_inited = true;
    }

    wifi_softap_on();

    /* Create AP/STA netifs (only once) */
    if (!s_netif_ap) {
        s_netif_ap = wifi_init_softap();
    }
    if (!s_netif_sta) {
        s_netif_sta = wifi_init_sta(wifi_ssid, wifi_pass);
    }

    esp_err_t err = esp_wifi_start();
    if (err != ESP_OK && err != ESP_ERR_WIFI_CONN) {
        ESP_ERROR_CHECK(err);
    }

    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    if (s_netif_sta) {
        err = esp_netif_set_hostname(s_netif_sta, hostname);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "esp_netif_set_hostname failed: %s", esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "Hostname: %s", hostname);
        }
    }

    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mac));
    snprintf(s_mac_addr, sizeof(s_mac_addr), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    return s_netif_sta;
}

EventBits_t wifi_connect(void)
{
    if (!s_wifi_event_group) {
        return 0;
    }
    return xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
}
