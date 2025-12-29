#include <cstdlib>
#include <cstring>
#include <string>

#include "connect.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "nvs_config.h"

#include "global_state.h"
#include "macros.h"
#include "utils.h"

static const char *TAG = "discord";

#define ALERTER_QUEUE_LEN 4

#define DISCORD_TASK_PRIO 5

#ifndef DIFF_STRING_SIZE
#define DIFF_STRING_SIZE 12
#endif

Alerter::Alerter() : m_payloadBuffer(nullptr)
{
    // NOP
}

bool Alerter::init()
{
    m_payloadBuffer = (char *) MALLOC(payloadBufferSize);
    if (!m_payloadBuffer) {
        ESP_LOGE(TAG, "Error creating payload buffer");
        return false;
    }

    m_msgQueue = xQueueCreate(ALERTER_QUEUE_LEN, sizeof(alerter_msg_t));
    if (!m_msgQueue) {
        ESP_LOGE(TAG, "Failed to create queue");
        return false;
    }

    loadConfig();
    return true;
}

void Alerter::loadConfig()
{
    safe_free(m_webhookUrl);
    safe_free(m_host);

    m_host = Config::getHostname();
    m_webhookUrl = Config::getDiscordWebhook();
    m_wdtAlertEnabled = Config::isDiscordWatchdogAlertEnabled();
    m_blockFoundAlertEnabled = Config::isDiscordBlockFoundAlertEnabled();
    m_bestDiffAlertEnabled = Config::isDiscordBestDiffAlertEnabled();
}

DiscordAlerter::DiscordAlerter() : Alerter()
{
    // NOP
}


bool DiscordAlerter::init()
{
    return Alerter::init();
}

bool DiscordAlerter::httpPost(const char *message)
{
    if (m_webhookUrl == nullptr || message == nullptr) {
        ESP_LOGE(TAG, "Webhook URL or message is null");
        return false;
    }

    if (!m_payloadBuffer) {
        ESP_LOGE(TAG, "payload buffer is nullptr");
        return false;
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

bool DiscordAlerter::enqueueMessage(const char *message)
{
    char ip[20] = {0};
    connect_get_ip_addr(ip, sizeof(ip));
    const char *mac = SYSTEM_MODULE.getMacAddress();

    alerter_msg_t msg = {};

    snprintf(msg.message, sizeof(msg.message) - 1, "%s\\n```\\nHostname: %s\\nIP:       %s\\nMAC:      %s\\n```", message,
             m_host ? m_host : "unknown", ip, mac ? mac : "unknown");

    ESP_LOGW(TAG, "enqueued: %s", msg.message);

    return xQueueSend(m_msgQueue, &msg, 0) == pdTRUE;
}

bool DiscordAlerter::sendWatchdogAlert()
{
    if (!m_wdtAlertEnabled) {
        ESP_LOGI(TAG, "discord watchdog alert not enabled");
        return false;
    }

    return enqueueMessage("Device rebooted because there was no share for more than 1h!");
}

bool DiscordAlerter::sendBlockFoundAlert(double diff, double networkDiff)
{
    if (!m_blockFoundAlertEnabled) {
        ESP_LOGI(TAG, "discord block found alert not enabled");
        return false;
    }

    char diffStr[DIFF_STRING_SIZE];
    char netStr[DIFF_STRING_SIZE];
    suffixString((uint64_t) diff, diffStr, DIFF_STRING_SIZE, 0);
    suffixString((uint64_t) networkDiff, netStr, DIFF_STRING_SIZE, 0);

    char base[192];
    snprintf(base, sizeof(base) - 1, ":tada: Block found!\\nDiff: %s (network: %s)", diffStr, netStr);

    return enqueueMessage(base);
}

bool DiscordAlerter::sendBestDifficultyAlert(double diff, double networkDiff)
{
    if (!m_bestDiffAlertEnabled) {
        return false;
    }

    char bestStr[DIFF_STRING_SIZE];
    char netStr[DIFF_STRING_SIZE];

    suffixString((uint64_t) diff, bestStr, DIFF_STRING_SIZE, 0);
    suffixString((uint64_t) networkDiff, netStr, DIFF_STRING_SIZE, 0);

    char base[160];
    snprintf(base, sizeof(base) - 1,
             ":chart_with_upwards_trend: New *best difficulty* found!\\n"
             "Diff: %s (network: %s)",
             bestStr, netStr);

    return enqueueMessage(base);
}

bool DiscordAlerter::sendTestMessage()
{
    return enqueueMessage("This is a test message!");
}

void DiscordAlerter::taskWrapper(void *pv) {
    DiscordAlerter *alerter = static_cast<DiscordAlerter*>(pv);
    alerter->task();
}

void DiscordAlerter::task() {
    ESP_LOGI(TAG, "Discord alerter started");

    alerter_msg_t msg;
    while (1) {
        if (xQueueReceive(m_msgQueue, &msg, portMAX_DELAY) == pdTRUE) {
            httpPost(msg.message);
        }
    }
}

void DiscordAlerter::start()
{
    if (!init()) {
        ESP_LOGE(TAG, "error init discord alerter!");
        return;
    }

    xTaskCreatePSRAM(DiscordAlerter::taskWrapper, "discord_task", 8192, (void*) this, DISCORD_TASK_PRIO, NULL);

    ESP_LOGI(TAG, "Discord task started");
}