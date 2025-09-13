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

    // don't send the alertDiscordWebhook on the API
    //doc["alertDiscordWebhook"]  = alertDiscordWebhook;
    doc["alertDiscordWatchdogEnable"] = Config::isDiscordWatchdogAlertEnabled() ? 1 : 0;
    doc["alertDiscordBlockFoundEnable"] = Config::isDiscordBlockFoundAlertEnabled() ? 1 : 0;

    esp_err_t ret = sendJsonResponse(req, doc);

    doc.clear();
    free(alertDiscordWebhook);

    return ret;
}


esp_err_t POST_update_alert(httpd_req_t *req)
{
    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    if (set_cors_headers(req) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    int total_len = req->content_len;
    int cur_len = 0;
    char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
    int received = 0;

    if (total_len >= SCRATCH_BUFSIZE) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Content too long");
        return ESP_FAIL;
    }

    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len);
        if (received <= 0) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive body");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';

    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);

    DeserializationError error = deserializeJson(doc, buf);
    if (error) {
        ESP_LOGE(TAG, "JSON parsing failed: %s", error.c_str());
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    if (doc["alertDiscordWebhook"].is<const char*>()) {
        Config::setDiscordWebhook(doc["alertDiscordWebhook"].as<const char*>());
    }
    if (doc["alertDiscordWatchdogEnable"].is<bool>()) {
        Config::setDiscordWatchdogAlertEnabled(doc["alertDiscordWatchdogEnable"].as<bool>());
    }
    if (doc["alertDiscordBlockFoundEnable"].is<bool>()) {
        Config::setDiscordAlertBlockFoundEnabled(doc["alertDiscordBlockFoundEnable"].as<bool>());
    }


    doc.clear();

    httpd_resp_send_chunk(req, NULL, 0);

    // reload discord alerter config
    discordAlerter.loadConfig();

    return ESP_OK;
}

esp_err_t POST_test_alert(httpd_req_t *req)
{
    if (set_cors_headers(req) != ESP_OK) {
        httpd_resp_send_500(req);
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
