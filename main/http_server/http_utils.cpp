#include "esp_log.h"

#include "esp_err.h"
#include "ArduinoJson.h"

#include "http_utils.h"

static const char *TAG = "http_utils";

esp_err_t sendJsonResponse(httpd_req_t *req, JsonDocument &doc) {
    // Measure the size needed for the JSON text
    size_t jsonLength = measureJson(doc);
    // Allocate a buffer from PSRAM (or regular heap if you prefer)
    char *jsonOutput = (char*) heap_caps_malloc(jsonLength + 1, MALLOC_CAP_SPIRAM);
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
    heap_caps_free(jsonOutput);
    return ret;
}

