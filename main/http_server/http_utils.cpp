#include "esp_log.h"
#include <strings.h>

#include "ArduinoJson.h"
#include "esp_err.h"

#include "http_utils.h"
#include "macros.h"

static const char *TAG = "http_utils";

extern OTP otp;

esp_err_t sendJsonResponse(httpd_req_t *req, JsonDocument &doc)
{
    // Measure the size needed for the JSON text
    size_t jsonLength = measureJson(doc);
    // Allocate a buffer from PSRAM (or regular heap if you prefer)
    char *jsonOutput = (char *) MALLOC(jsonLength + 1);
    if (!jsonOutput) {
        ESP_LOGE(TAG, "Failed to allocate memory for JSON output");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation error");
        return ESP_FAIL;
    }
    // Serialize the JSON document into the allocated buffer
    serializeJson(doc, jsonOutput, jsonLength + 1);
    // Send the response
    esp_err_t ret = httpd_resp_sendstr(req, jsonOutput);
    // Free the allocated buffer
    FREE(jsonOutput);
    return ret;
}

esp_err_t getPostData(httpd_req_t *req)
{
    int total_len = req->content_len;
    int cur_len = 0;
    char *buf = ((rest_server_context_t *) (req->user_ctx))->scratch;
    int received = 0;

    if (total_len >= SCRATCH_BUFSIZE) {
        /* Respond with 500 Internal Server Error */
        ESP_LOGE(TAG, "content too long");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }

    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len);
        if (received <= 0) {
            /* Respond with 500 Internal Server Error */
            ESP_LOGE(TAG, "error receiving data");
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "error receiving data");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';
    return ESP_OK;
}

esp_err_t getJsonData(httpd_req_t *req, JsonDocument &doc)
{
    esp_err_t err = getPostData(req);
    if (err != ESP_OK) {
        return err;
    }

    char *buf = ((rest_server_context_t *) (req->user_ctx))->scratch;

    // Parse the JSON payload
    DeserializationError error = deserializeJson(doc, buf);
    if (error) {
        ESP_LOGE(TAG, "JSON parsing failed: %s", error.c_str());
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    return ESP_OK;
}

// --- helpers ---------------------------------------------------------------
static bool read_header_str(httpd_req_t *req, const char *name, char *out, size_t outlen)
{
    size_t len = httpd_req_get_hdr_value_len(req, name);
    if (len == 0 || len + 1 > outlen)
        return false;
    return httpd_req_get_hdr_value_str(req, name, out, outlen) == ESP_OK;
}

static bool read_totp_header(httpd_req_t *req, char *out, size_t outlen)
{
    return read_header_str(req, "X-TOTP", out, outlen);
}

static bool read_session_token(httpd_req_t *req, char *out, size_t outlen)
{
    return read_header_str(req, "X-OTP-Session", out, outlen);
}

static void get_client_fp(httpd_req_t *req, std::string &fp)
{
    char ua[256] = {0};
    if (read_header_str(req, "User-Agent", ua, sizeof(ua)))
        fp.assign(ua);
    else
        fp.clear();
}

static bool check_otp_or_session(httpd_req_t *req, bool force)
{
    if (!otp.isEnabled() && !force)
        return true;

    // example token
    // LGQ7I2GZ6L2WRCJXHJHA.7NGNYMKM5MK6WVI3NJHTOGL2NWPFI6SSHHFQTN6SJPEQ5WQVQZGA
    // 20 (payload) + 1 + 52 (bas32 sha256) = 73
    if (!force) {
        char sess[128] = {0};
        if (read_session_token(req, sess, sizeof(sess))) {
            if (otp.verifySessionToken(std::string(sess))) {
                return true;
            }
            // fallback to totp
        }
    }

    // totp
    char totp[16] = {0};
    if (read_totp_header(req, totp, sizeof(totp))) {
        if (otp.validate(totp))
            return true;
    }

    return false;
}

// Bequeme Variante: sendet 401 + WWW-Authenticate bei Failure
esp_err_t validateOTP(httpd_req_t *req, bool force)
{
    if (check_otp_or_session(req, force))
        return ESP_OK;

    ESP_LOGE(TAG, "totp validation failed");

    // sinnvolle Fehlerantwort zentral
    //httpd_resp_set_hdr(req, "WWW-Authenticate", "Bearer realm=\"NerdQX\", error=\"invalid_token\"");
    httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "OTP/Session required");
    return ESP_FAIL;
}