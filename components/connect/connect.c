#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_netif.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "lwip/err.h"
#include "lwip/lwip_napt.h"
#include "lwip/sys.h"
#include "nvs_flash.h"

#include "connect.h"

void MINER_set_wifi_status(wifi_status_t status, uint16_t retry_count);
void MINER_set_ap_status(bool state);

#if CONFIG_ESP_WPA3_SAE_PWE_HUNT_AND_PECK
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HUNT_AND_PECK
#define EXAMPLE_H2E_IDENTIFIER ""
#elif CONFIG_ESP_WPA3_SAE_PWE_HASH_TO_ELEMENT
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HASH_TO_ELEMENT
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#elif CONFIG_ESP_WPA3_SAE_PWE_BOTH
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_BOTH
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#endif

#if CONFIG_ESP_WIFI_AUTH_OPEN
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif

#define WIFI_MAXIMUM_RETRY 5

// FreeRTOS event group to signal when we are connected
static EventGroupHandle_t s_wifi_event_group;

static const char *TAG = "wifi station";

static int s_retry_num = 0;

static char s_ip_addr[20] = {0};
static char s_mac_addr[18] = {0};
static bool ip_valid = false;

// Serialize Wi-Fi API calls that change mode/config/start/stop
static SemaphoreHandle_t g_wifiMutex = NULL;
static inline void wifi_lock(void)   { if (g_wifiMutex) xSemaphoreTake(g_wifiMutex, portMAX_DELAY); }
static inline void wifi_unlock(void) { if (g_wifiMutex) xSemaphoreGive(g_wifiMutex); }

bool connect_get_ip_addr(char *buf, size_t buf_len)
{
    // Ensure output buffer is always terminated
    if (!buf || buf_len == 0) return false;
    strlcpy(buf, s_ip_addr, buf_len);
    return ip_valid;
}

const char* connect_get_mac_addr(void) {
    return s_mac_addr;
}

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        // Non-blocking: just request a connect
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_MAXIMUM_RETRY) {
            s_retry_num++;
            esp_wifi_connect();
            ESP_LOGI(TAG, "Retrying WiFi connection... (%d/%d)", s_retry_num, WIFI_MAXIMUM_RETRY);
            MINER_set_wifi_status(WIFI_RETRYING, s_retry_num);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGW(TAG, "Could not connect to WiFi (initial).");
            MINER_set_wifi_status(WIFI_CONNECT_FAILED, 0);
            // Continue trying in background for eventual recovery
            s_retry_num = 0;
            esp_wifi_connect();
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        snprintf(s_ip_addr, sizeof(s_ip_addr), IPSTR, IP2STR(&event->ip_info.ip));
        ip_valid = true;
        ESP_LOGI(TAG, "Device ip: %s", s_ip_addr);
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        MINER_set_wifi_status(WIFI_CONNECTED, 0);
    }
}

EventBits_t wifi_wait_connected_ms(TickType_t ticks)
{
    // Do not clear the bit here; other waiters may rely on it.
    return xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT,
        pdFALSE,   // do not clear on exit
        pdFALSE,
        ticks
    );
}

void generate_ssid(char *ssid, size_t len)  // >= 32 bytes recommended
{
    uint8_t mac[6];
    esp_wifi_get_mac(ESP_IF_WIFI_AP, mac);
    // Format last 2 bytes of MAC; keep SSID short/safe
    snprintf(ssid, len, "Nerdaxe_%02X%02X", mac[4], mac[5]);
}

esp_netif_t *wifi_init_softap(void)
{
    esp_netif_t *esp_netif_ap = esp_netif_create_default_wifi_ap();

    char ssid_with_mac[32] = {0};
    generate_ssid(ssid_with_mac, sizeof(ssid_with_mac));

    wifi_config_t wifi_ap_config = {0};  // zero-init
    strlcpy((char *)wifi_ap_config.ap.ssid, ssid_with_mac, sizeof(wifi_ap_config.ap.ssid));
    wifi_ap_config.ap.ssid_len = strlen((char*)wifi_ap_config.ap.ssid);
    wifi_ap_config.ap.channel = 1;
    wifi_ap_config.ap.max_connection = 30;
    wifi_ap_config.ap.authmode = WIFI_AUTH_OPEN;
    // wifi_ap_config.ap.pmf_cfg.required = false;

    wifi_lock();
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config));
    wifi_unlock();

    return esp_netif_ap;
}

void wifi_softap_off(void)
{
    ESP_LOGI(TAG, "ESP_WIFI Access Point Off");
    wifi_lock();
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    wifi_unlock();
    MINER_set_ap_status(false);
}

void wifi_softap_on(void)
{
    ESP_LOGI(TAG, "ESP_WIFI Access Point On");
    wifi_lock();
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_start());
    wifi_unlock();
    MINER_set_ap_status(true);
}

// Initialize Wi-Fi station config (does not start Wi-Fi)
esp_netif_t *wifi_init_sta(const char *wifi_ssid, const char *wifi_pass)
{
    esp_netif_t *esp_netif_sta = esp_netif_create_default_wifi_sta();

    wifi_config_t wifi_sta_config = {0};  // zero-init
    wifi_sta_config.sta.threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD;
    // wifi_sta_config.sta.sae_pwe_h2e = ESP_WIFI_SAE_MODE;
    // wifi_sta_config.sta.sae_h2e_identifier = EXAMPLE_H2E_IDENTIFIER;
    wifi_sta_config.sta.btm_enabled = 1;
    wifi_sta_config.sta.rm_enabled = 1;
    wifi_sta_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    wifi_sta_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    wifi_sta_config.sta.pmf_cfg.capable = true;
    wifi_sta_config.sta.pmf_cfg.required = false;

    // Copy credentials safely
    strlcpy((char *)wifi_sta_config.sta.ssid, wifi_ssid, sizeof(wifi_sta_config.sta.ssid));
    strlcpy((char *)wifi_sta_config.sta.password, wifi_pass, sizeof(wifi_sta_config.sta.password));

    wifi_lock();
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config));
    wifi_unlock();

    ESP_LOGI(TAG, "wifi_init_sta finished.");
    return esp_netif_sta;
}

// Apply new STA credentials safely at runtime (optional helper)
esp_err_t wifi_apply_sta_config(const char *ssid, const char *pass)
{
    wifi_config_t cfg = {0};
    strlcpy((char*)cfg.sta.ssid, ssid, sizeof(cfg.sta.ssid));
    strlcpy((char*)cfg.sta.password, pass, sizeof(cfg.sta.password));
    cfg.sta.threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD;
    cfg.sta.pmf_cfg.capable = true;
    cfg.sta.pmf_cfg.required = false;

    wifi_lock();
    esp_err_t err = ESP_OK;
    // Tear down gracefully before applying new config
    err |= esp_wifi_disconnect();
    err |= esp_wifi_stop();
    err |= esp_wifi_set_mode(WIFI_MODE_STA);          // or WIFI_MODE_APSTA if needed
    err |= esp_wifi_set_config(WIFI_IF_STA, &cfg);
    err |= esp_wifi_start();
    err |= esp_wifi_connect();
    wifi_unlock();
    return err;
}

void wifi_init(const char *wifi_ssid, const char *wifi_pass, const char *hostname)
{
    s_wifi_event_group = xEventGroupCreate();

    strlcpy(s_ip_addr, "0.0.0.0", sizeof(s_ip_addr));

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create API serialization mutex early
    g_wifiMutex = xSemaphoreCreateMutex();

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

    // Initialize Wi-Fi driver
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Set initial mode to APSTA in a safe way (before start)
    wifi_lock();
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    wifi_unlock();

    // Initialize AP/STA configs
    ESP_LOGI(TAG, "ESP_WIFI Access Point On");
    esp_netif_t *esp_netif_ap  = wifi_init_softap();
    (void)esp_netif_ap;

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    esp_netif_t *esp_netif_sta = wifi_init_sta(wifi_ssid, wifi_pass);
    (void)esp_netif_sta;

    // Start Wi-Fi
    wifi_lock();
    ESP_ERROR_CHECK(esp_wifi_start());
    // Disable power savings for best performance
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    wifi_unlock();

    // Set Hostname
    esp_err_t err = esp_netif_set_hostname(esp_netif_sta, hostname);
    if (err != ERR_OK) {
        ESP_LOGW(TAG, "esp_netif_set_hostname failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "ESP_WIFI setting hostname to: %s", hostname);
    }

    // Cache MAC address string
    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mac));
    snprintf(s_mac_addr, sizeof(s_mac_addr), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    ESP_LOGI(TAG, "wifi_init finished.");
}

EventBits_t wifi_connect(void)
{
    // Wait until either connected or initial attempts failed
    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE, pdFALSE, portMAX_DELAY);
    return bits;
}
