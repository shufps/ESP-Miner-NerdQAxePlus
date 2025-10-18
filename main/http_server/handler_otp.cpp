#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_timer.h"

#include "ArduinoJson.h"

#include "global_state.h"
#include "http_cors.h"
#include "http_utils.h"
#include "nvs_config.h"
#include "psram_allocator.h"

#include "../displays/ui_ipc.h"
#include "ping_task.h"

static const char *TAG = "http_otp";

static bool enrollmengActive = false;

esp_err_t POST_create_otp(httpd_req_t *req)
{
    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    if (set_cors_headers(req) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    if (otp.isEnabled()) {
        return httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "OTP is enabled");
    }

    // trigger QR creation
    esp_err_t err = otp.startEnrollment();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "error starting otp enrollment");
    }

    // send command to the UI task to show the QR on the screen
    ui_send_show_qr();

    httpd_resp_send_chunk(req, NULL, 0);

    return ESP_OK;
}

esp_err_t PATCH_update_otp(httpd_req_t *req)
{
    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    // Set CORS headers
    if (set_cors_headers(req) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // disable enrollment
    // no effect if it wasn't started
    otp.disableEnrollment();

    // hide in any case
    ui_send_hide_qr();

    if (validateOTP(req, true) != ESP_OK) {
        ESP_LOGE(TAG, "totp validation failed");
        httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "totp missing or invalid");
        return ESP_FAIL;
    }

    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);

    esp_err_t err = getJsonData(req, doc);
    if (err != ESP_OK) {
        return err;
    }

    if (!doc["enabled"].is<bool>()) {
        ESP_LOGE(TAG, "invalid enabled state");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "invalid enabled state");
        return ESP_FAIL;
    }

    bool enabled = doc["enabled"].as<bool>();

    // set the enable to the desired state
    otp.setEnable(enabled);

    // after validation we persist the enable flag
    // and the secret what finishes the registration
    otp.saveSettings();

    doc.clear();

    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}
