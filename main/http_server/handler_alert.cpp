#include "esp_http_server.h"
#include "esp_log.h"
#include "ArduinoJson.h"

#include "global_state.h"
#include "psram_allocator.h"
#include "nvs_config.h"
#include "http_cors.h"
#include "http_utils.h"
#include "discord.h"

static const char* TAG = "http_alert";

esp_err_t GET_alert_info(httpd_req_t *req)
{
    // close connection when out of scope
    ConGuard g(http_server, req);

    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    httpd_resp_set_type(req, "application/json");

    if (set_cors_headers(req) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    char *alertDiscordWebhook = Config::getDiscordWebhook();

    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);

    doc["hasWebhook"] = (alertDiscordWebhook && alertDiscordWebhook[0] != '\0');
    doc["watchdogEnable"] = Config::isDiscordWatchdogAlertEnabled() ? 1 : 0;
    doc["blockFoundEnable"] = Config::isDiscordBlockFoundAlertEnabled() ? 1 : 0;
    doc["bestDiffEnable"] = Config::isDiscordBestDiffAlertEnabled() ? 1 : 0;
    doc["coinbaseVerifyEnable"] = Config::isDiscordCoinbaseVerifyAlertEnabled() ? 1 : 0;
    doc["showBlockFoundScreen"] = Config::isShowBlockFoundEnabled() ? 1 : 0;

    esp_err_t ret = sendJsonResponse(req, doc);

    doc.clear();
    free(alertDiscordWebhook);

    return ret;
}


esp_err_t POST_update_alert(httpd_req_t *req)
{
    // close connection when out of scope
    ConGuard g(http_server, req);

    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    if (set_cors_headers(req) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    if (validateOTP(req) != ESP_OK) {
        return ESP_FAIL;
    }

    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);

    esp_err_t err = getJsonData(req, doc);
    if (err != ESP_OK) {
        return err;
    }

    if (doc["webhookUrl"].is<const char*>()) {
        Config::setDiscordWebhook(doc["webhookUrl"].as<const char*>());
    }
    if (doc["watchdogEnable"].is<bool>()) {
        Config::setDiscordWatchdogAlertEnabled(doc["watchdogEnable"].as<bool>());
    }
    if (doc["blockFoundEnable"].is<bool>()) {
        Config::setDiscordAlertBlockFoundEnabled(doc["blockFoundEnable"].as<bool>());
    }
    if (doc["bestDiffEnable"].is<bool>()) {
        Config::setDiscordAlertBestDiffEnabled(doc["bestDiffEnable"].as<bool>());
    }
    if (doc["coinbaseVerifyEnable"].is<bool>()) {
        Config::setDiscordCoinbaseVerifyAlertEnabled(doc["coinbaseVerifyEnable"].as<bool>());
    }
    if (doc["showBlockFoundScreen"].is<bool>()) {
        Config::setShowBlockFoundEnabled(doc["showBlockFoundScreen"].as<bool>());
    }

    doc.clear();

    httpd_resp_send_chunk(req, NULL, 0);

    // reload discord alerter config
    discordAlerter.loadConfig();

    // reload config
    SYSTEM_MODULE.loadSettings();

    return ESP_OK;
}

esp_err_t POST_test_alert(httpd_req_t *req)
{
    // close connection when out of scope
    ConGuard g(http_server, req);

    if (set_cors_headers(req) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    if (validateOTP(req) != ESP_OK) {
        return ESP_FAIL;
    }

    bool success = discordAlerter.sendTestMessage();

    if (success) {
        httpd_resp_sendstr(req, "ok");
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Send failed");
    }

    return ESP_OK;
}
