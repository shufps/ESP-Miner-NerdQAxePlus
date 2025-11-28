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

esp_err_t POST_create_otp(httpd_req_t *req)
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

    if (otp.isEnabled()) {
        return httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "OTP is enabled");
    }

    // trigger QR creation
    esp_err_t err = otp.startEnrollment();

    // enrollment already started? Don't return an error but
    // don't create a new QR either
    if (err == ESP_ERR_NOT_FINISHED) {
        httpd_resp_send_chunk(req, NULL, 0);
        return ESP_OK;
    }

    // not ok? return error
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "error starting otp enrollment");
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "otp enrollment failed");
    }

    // send command to the UI task to show the QR on the screen
    ui_send_show_qr();

    httpd_resp_send_chunk(req, NULL, 0);

    return ESP_OK;
}

esp_err_t PATCH_update_otp(httpd_req_t *req)
{
    // close connection when out of scope
    ConGuard g(http_server, req);

    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    // Set CORS headers
    if (set_cors_headers(req) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // force flag is for ignoring the enabled flag but
    // it also insists on OTP validation and switches off session token check
    if (validateOTP(req, true) != ESP_OK) {
        ESP_LOGE(TAG, "totp validation failed");
        httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "totp missing or invalid");
        return ESP_FAIL;
    }

    // disable enrollment
    // no effect if it wasn't started
    otp.disableEnrollment();

    // hide in any case
    ui_send_hide_qr();

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


esp_err_t GET_otp_status(httpd_req_t *req)
{
    // close connection when out of scope
    ConGuard g(http_server, req);

    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    // Set CORS headers
    if (set_cors_headers(req) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);

    doc["enabled"] = otp.isEnabled();

    esp_err_t ret = sendJsonResponse(req, doc);
    doc.clear();
    return ret;
}


// --- utils ------------------------------------------------------------------

static bool read_header_str(httpd_req_t *req, const char *name, char *out, size_t outlen) {
    size_t len = httpd_req_get_hdr_value_len(req, name);
    if (len == 0 || len + 1 > outlen) return false;
    return httpd_req_get_hdr_value_str(req, name, out, outlen) == ESP_OK;
}

static uint32_t clamp_ttl_ms(uint64_t v, uint32_t min_ms, uint32_t max_ms) {
    if (v < min_ms) return min_ms;
    if (v > max_ms) return max_ms;
    return (uint32_t)v;
}

esp_err_t POST_create_otp_session(httpd_req_t *req)
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

    // otp must be enabled
    if (!otp.isEnabled()) {
        return httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "OTP disabled");
    }

    // read otp from the header
    char totp[16] = {0};
    if (!read_header_str(req, "X-TOTP", totp, sizeof(totp))) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing X-TOTP header");
    }

    // validate
    if (!otp.validate(totp)) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Invalid TOTP");
    }

    // create session token - default 24h
    uint32_t ttlSeconds = 24 * 3600;
    std::string token = otp.mintSessionToken(ttlSeconds);
    if (token.empty()) {
        ESP_LOGE("http_otp", "createSessionToken failed");
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Session error");
    }
    uint64_t expires_ms = now_ms() + (uint64_t) ttlSeconds * 1000ull;

    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);
    doc["token"]     = token;
    doc["ttlMs"]     = ttlSeconds * 1000;
    doc["expiresAt"] = (double)expires_ms;

    std::string out;
    serializeJson(doc, out);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, out.c_str());
    return ESP_OK;
}
