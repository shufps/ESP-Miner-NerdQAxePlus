#include <cstdlib>
#include <cstring>

#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "nvs_config.h"

#include "global_state.h"

static const char *TAG = "discord";

static bool sendDiscordRaw(const char* webhookUrl, const char* message)
{
    if (webhookUrl == nullptr || message == nullptr) {
        ESP_LOGE(TAG, "Webhook URL or message is null");
        return false;
    }

    char post_data[512];
    snprintf(post_data, sizeof(post_data), "{\"content\": \"%s\"}", message);

    esp_http_client_config_t config = {};
    config.url = webhookUrl;
    config.method = HTTP_METHOD_POST;
    config.timeout_ms = 5000;
    config.crt_bundle_attach = esp_crt_bundle_attach;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        return false;
    }

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP POST failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return false;
    }

    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (status >= 200 && status < 300) {
        ESP_LOGI(TAG, "Discord message sent successfully (HTTP %d)", status);
        return true;
    } else {
        ESP_LOGE(TAG, "Discord responded with HTTP %d", status);
        return false;
    }
}

bool sendDiscordMessageIfEnabled(const char *message)
{
    if (!Config::isDiscordAlertEnabled()) {
        ESP_LOGI(TAG, "Alert disabled – skipping Discord send");
        return false;
    }

    char *url = Config::getDiscordWebhook();
    if (url == nullptr || strlen(url) == 0) {
        ESP_LOGW(TAG, "No Discord webhook configured");
        if (url)
            free(url);
        return false;
    }

    const char *ip = SYSTEM_MODULE.getIPAddress();
    const char *mac = SYSTEM_MODULE.getMacAddress();
    char *hostname = Config::getHostname();

    char fullMessage[768];
    snprintf(fullMessage, sizeof(fullMessage), "%s\n```\nHostname: %s\nIP:      %s\nMAC:     %s\n```", message,
             hostname ? hostname : "unknown", ip ? ip : "unknown", mac ? mac : "unknown");

    bool result = sendDiscordRaw(url, fullMessage);

    free(url);
    if (hostname)
        free(hostname);
    return result;
}

bool sendDiscordTestMessage()
{
    char *url = Config::getDiscordWebhook();
    if (url == nullptr || strlen(url) == 0) {
        ESP_LOGW(TAG, "No Discord webhook configured");
        if (url)
            free(url);
        return false;
    }

    bool result = sendDiscordRaw(url, "This is a test message!");
    free(url);
    return result;
}
