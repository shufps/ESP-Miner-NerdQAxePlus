#include "esp_log.h"

#include "esp_err.h"
#include "ArduinoJson.h"

#include "http_utils.h"
#include "macros.h"

static const char *TAG = "http_utils";

esp_err_t sendJsonResponse(httpd_req_t *req, JsonDocument &doc) {
    // Measure the size needed for the JSON text
    size_t jsonLength = measureJson(doc);
    // Allocate a buffer from PSRAM (or regular heap if you prefer)
    char *jsonOutput = (char*) MALLOC(jsonLength + 1);
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

esp_err_t getPostData(httpd_req_t *req) {
    int total_len = req->content_len;
    int cur_len = 0;
    char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
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

esp_err_t getJsonData(httpd_req_t *req, JsonDocument &doc) {
    esp_err_t err = getPostData(req);
    if (err != ESP_OK) {
        return err;
    }

    char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;

    // Parse the JSON payload
    DeserializationError error = deserializeJson(doc, buf);
    if (error) {
        ESP_LOGE(TAG, "JSON parsing failed: %s", error.c_str());
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    return ESP_OK;
}
