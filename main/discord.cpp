#include <cstdlib>
#include <cstring>
#include <string>

#include "connect.h"
#include "crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <cinttypes>
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
    m_coinbaseVerifyAlertEnabled = Config::isDiscordCoinbaseVerifyAlertEnabled();
    m_heartbeatHours = Config::getAlertHeartbeatHours();
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
    config.crt_bundle_attach = crt_bundle_attach;

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
    NETWORK.getPreferredIpAddr(ip, sizeof(ip), nullptr);
    const char *mac = SYSTEM_MODULE.getMacAddress();

    // on the heap: alerts are also sent from tasks with small stacks
    alerter_msg_t *msg = (alerter_msg_t *) MALLOC(sizeof(alerter_msg_t));
    if (!msg) {
        ESP_LOGE(TAG, "no memory for alert message");
        return false;
    }

    snprintf(msg->message, sizeof(msg->message) - 1, "%s\\n```\\nHostname: %s\\nIP:       %s\\nMAC:      %s\\n```", message,
             m_host ? m_host : "unknown", ip, mac ? mac : "unknown");

    ESP_LOGW(TAG, "enqueued: %s", msg->message);

    bool ok = xQueueSend(m_msgQueue, msg, 0) == pdTRUE;
    free(msg);
    return ok;
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

bool DiscordAlerter::sendCoinbaseVerifyFailed(int pool, const coinbase_result_t *cb, int mode)
{
    if (!m_coinbaseVerifyAlertEnabled) {
        return false;
    }

    char base[192];

    if (cb->user_value_satoshis == 0) {
        snprintf(base, sizeof(base) - 1,
                 ":warning: Coinbase verification failed for pool %d!\\n"
                 "Your address was **not found** in the coinbase transaction.",
                 pool + 1);
    } else if (mode >= 2 && cb->total_value_satoshis > 0) {
        uint32_t fee_pct_x100 = (uint32_t)(((cb->total_value_satoshis - cb->user_value_satoshis) * 10000ULL) / cb->total_value_satoshis);
        snprintf(base, sizeof(base) - 1,
                 ":warning: Coinbase verification failed for pool %d!\\n"
                 "Pool fee: **%lu.%02lu%%** exceeds configured limit.",
                 pool + 1,
                 fee_pct_x100 / 100, fee_pct_x100 % 100);
    } else {
        snprintf(base, sizeof(base) - 1,
                 ":warning: Coinbase verification failed for pool %d!",
                 pool + 1);
    }

    return enqueueMessage(base);
}

static void formatHashrate(double ghs, char *buf, size_t len)
{
    if (ghs >= 1000.0) {
        snprintf(buf, len, "%.2f TH/s", ghs / 1000.0);
    } else {
        snprintf(buf, len, "%.1f GH/s", ghs);
    }
}

bool DiscordAlerter::sendHeartbeat()
{
    Board *board = SYSTEM_MODULE.getBoard();
    History *history = SYSTEM_MODULE.getHistory();
    bool shutdown = POWER_MANAGEMENT_MODULE.isShutdown();

    char hr[16];
    char hr1h[16];
    formatHashrate(shutdown ? 0.0 : SYSTEM_MODULE.getCurrentHashrate(), hr, sizeof(hr));
    formatHashrate((shutdown || !history) ? 0.0 : history->getCurrentHashrate1h(), hr1h, sizeof(hr1h));

    uint32_t uptime = (esp_timer_get_time() - SYSTEM_MODULE.getStartTime()) / 1000000;

    char base[ALERTER_MESSAGE_LEN - 128];
    int n = snprintf(base, sizeof(base),
                     ":heartbeat: Mining heartbeat%s\\n```\\n"
                     "Hashrate: %s (1h: %s)\\n"
                     "Power:    %.1f W\\n"
                     "ASIC:     %.1f °C\\n",
                     shutdown ? " - **shut down**" : "", hr, hr1h,
                     POWER_MANAGEMENT_MODULE.getPower(), POWER_MANAGEMENT_MODULE.getChipTempMax());

    float vrTemp = POWER_MANAGEMENT_MODULE.getVRTemp();
    if (vrTemp > 0 && n < (int) sizeof(base)) {
        n += snprintf(base + n, sizeof(base) - n, "VR:       %.1f °C\\n", vrTemp);
    }

    int numFans = board ? board->getNumFans() : 1;
    for (int ch = 0; ch < numFans && n < (int) sizeof(base); ch++) {
        const char *label = numFans > 1 ? (ch ? "Fan 2:" : "Fan 1:") : "Fan:";
        n += snprintf(base + n, sizeof(base) - n, "%-9s %d %% (%d RPM)\\n", label,
                      (int) POWER_MANAGEMENT_MODULE.getFanPerc(ch), (int) POWER_MANAGEMENT_MODULE.getFanRPM(ch));
    }

    if (n < (int) sizeof(base)) {
        snprintf(base + n, sizeof(base) - n,
                 "Shares:   %" PRIu64 " accepted, %" PRIu64 " rejected\\n"
                 "Best:     %s (session: %s)\\n"
                 "Uptime:   %" PRIu32 "d %" PRIu32 "h %" PRIu32 "m\\n```",
                 STRATUM_MANAGER->getSharesAccepted(), STRATUM_MANAGER->getSharesRejected(),
                 STRATUM_MANAGER->getBestDiffString(), STRATUM_MANAGER->getBestSessionDiffString(),
                 uptime / 86400, (uptime % 86400) / 3600, (uptime % 3600) / 60);
    }

    return enqueueMessage(base);
}

// Sends the heartbeat every m_heartbeatHours. The interval starts when it
// is enabled or changed (and at boot), so the first one comes one interval later.
void DiscordAlerter::checkHeartbeat()
{
    uint16_t hours = m_heartbeatHours;
    int64_t now = esp_timer_get_time();

    if (hours != m_activeHeartbeatHours) {
        m_activeHeartbeatHours = hours;
        m_lastHeartbeatUs = now;
        return;
    }

    if (!hours || now - m_lastHeartbeatUs < (int64_t) hours * 3600LL * 1000000LL) {
        return;
    }

    m_lastHeartbeatUs = now;
    if (!sendHeartbeat()) {
        ESP_LOGW(TAG, "heartbeat could not be queued");
    }
}

void DiscordAlerter::taskWrapper(void *pv) {
    DiscordAlerter *alerter = static_cast<DiscordAlerter*>(pv);
    alerter->task();
}

void DiscordAlerter::task() {
    ESP_LOGI(TAG, "Discord alerter started");

    alerter_msg_t msg;
    while (1) {
        // wake up at least once a minute to check the heartbeat interval
        if (xQueueReceive(m_msgQueue, &msg, pdMS_TO_TICKS(60000)) == pdTRUE) {
            httpPost(msg.message);
        }
        checkHeartbeat();
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