#include <cstdlib>
#include <cstring>
#include <string>

#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "nvs_config.h"
#include "connect.h"

#include "global_state.h"
#include "macros.h"

static const char *TAG = "discord";

#ifndef DIFF_STRING_SIZE
#define DIFF_STRING_SIZE 32
#endif

Alerter::Alerter() : m_messageBuffer(nullptr), m_payloadBuffer(nullptr)
{
    // NOP
}

void Alerter::init() {
    m_messageBuffer = (char *) MALLOC(messageBufferSize);
    m_payloadBuffer = (char *) MALLOC(payloadBufferSize);
}

DiscordAlerter::DiscordAlerter() : Alerter(), m_webhookUrl(nullptr)
{
    // NOP
}

void DiscordAlerter::loadConfig()
{
    if (m_webhookUrl) {
        free(m_webhookUrl);
        m_webhookUrl = nullptr;
    }

    m_webhookUrl = Config::getDiscordWebhook();
}

void DiscordAlerter::init() {
    Alerter::init();
}

bool DiscordAlerter::sendRaw(const char *message)
{
    if (m_webhookUrl == nullptr || message == nullptr) {
        ESP_LOGE(TAG, "Webhook URL or message is null");
        return false;
    }

    if (!m_payloadBuffer) {
        ESP_LOGE(TAG, "payload buffer is nullptr");
    }

    ESP_LOGI(TAG, "discord message: %s", message);

    snprintf(m_payloadBuffer, payloadBufferSize - 1, "{\"content\": \"%s\"}", message);

    ESP_LOGD(TAG, "discord payload: '%s'", m_payloadBuffer);

    esp_http_client_config_t config = {};
    config.url = m_webhookUrl;
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
    esp_http_client_set_post_field(client, m_payloadBuffer, strlen(m_payloadBuffer));

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

bool DiscordAlerter::sendMessage(const char *message)
{
    if (!m_messageBuffer) {
        ESP_LOGE(TAG, "message buffer is nullptr");
        return false;
    }

    char ip[20] = {0};
    apsta_get_sta_ip_str(ip, sizeof(ip));
    //ESP_LOGI(TAG, "IP: %s", ip);
    const char *mac = SYSTEM_MODULE.getMacAddress();
    //ESP_LOGI(TAG, "MAC: %s", mac);
    char *hostname = Config::getHostname();

    //ESP_LOGI(TAG, "IP: %s", ip.c_str());
    //ESP_LOGI(TAG, "MAC: %s", mac.c_str());
    //ESP_LOGI(TAG, "Hostname: %s", hostname);

    snprintf(m_messageBuffer, messageBufferSize - 1, "%s\\n```\\nHostname: %s\\nIP:       %s\\nMAC:      %s\\n```", message,
             hostname ? hostname : "unknown", ip, mac ? mac : "unknown");

    free(hostname);

    return sendRaw(m_messageBuffer);
}

bool DiscordAlerter::sendWatchdogAlert() {
    if (!Config::isDiscordWatchdogAlertEnabled()) {
        ESP_LOGI(TAG, "discord watchdog alert not enabled");
        return false;
    }

    return discordAlerter.sendMessage("Device rebootet because there was no share for more than 1h!");
}

bool DiscordAlerter::sendBlockFoundAlert(double diff, double networkDiff)
{
    if (!Config::isDiscordBlockFoundAlertEnabled()) {
        ESP_LOGI(TAG, "discord block found alert not enabled");
        return false;
    }

    char diffStr[DIFF_STRING_SIZE];
    char netStr[DIFF_STRING_SIZE];
    SYSTEM_MODULE.suffixString((uint64_t)diff,        diffStr, DIFF_STRING_SIZE, 0);
    SYSTEM_MODULE.suffixString((uint64_t)networkDiff, netStr,  DIFF_STRING_SIZE, 0);

    char base[192];
    snprintf(base, sizeof(base) - 1,
             ":tada: Block found!\\nDiff: %s (network: %s)",
             diffStr, netStr);

    return sendMessage(base);
}


bool DiscordAlerter::sendTestMessage()
{
    return sendMessage("This is a test message!");
}
