#include "esp_http_server.h"
#include "esp_log.h"

#include "ArduinoJson.h"

#include "psram_allocator.h"
#include "nvs_config.h"
#include "http_cors.h"
#include "http_utils.h"

static const char* TAG="http_influx";

/* Simple handler for getting system handler */
esp_err_t GET_influx_info(httpd_req_t *req)
{
    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    httpd_resp_set_type(req, "application/json");

    // Set CORS headers
    if (set_cors_headers(req) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Retrieve configuration strings from NVS
    char *influxURL    = Config::getInfluxURL();
    char *influxBucket = Config::getInfluxBucket();
    char *influxOrg    = Config::getInfluxOrg();
    char *influxPrefix = Config::getInfluxPrefix();

    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);

    // Fill the JSON object with values
    doc["influxURL"]    = influxURL;
    doc["influxPort"]   = Config::getInfluxPort();
    doc["influxBucket"] = influxBucket;
    doc["influxOrg"]    = influxOrg;
    doc["influxPrefix"] = influxPrefix;
    doc["influxEnable"] = Config::isInfluxEnabled() ? 1 : 0;

    // Serialize the JSON document into a string (using Arduino's String type)
    esp_err_t ret = sendJsonResponse(req, doc);

    doc.clear();

    // Free temporary strings from NVS retrieval
    free(influxURL);
    free(influxBucket);
    free(influxOrg);
    free(influxPrefix);

    return ret;
}


esp_err_t PATCH_update_influx(httpd_req_t *req)
{
    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    // Set CORS headers
    if (set_cors_headers(req) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    int total_len = req->content_len;
    int cur_len = 0;
    char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
    int received = 0;

    if (total_len >= SCRATCH_BUFSIZE) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }

    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len);
        if (received <= 0) {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';

    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);

    // Parse the JSON payload
    DeserializationError error = deserializeJson(doc, buf);
    if (error) {
        ESP_LOGE(TAG, "JSON parsing failed: %s", error.c_str());
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    // Check and apply each setting if the key exists and has the correct type
    if (doc["influxEnable"].is<bool>()) {
        Config::setInfluxEnabled(doc["influxEnable"].as<bool>());
    }
    if (doc["influxURL"].is<const char*>()) {
        Config::setInfluxURL(doc["influxURL"].as<const char*>());
    }
    if (doc["influxPort"].is<uint16_t>()) {
        Config::setInfluxPort(doc["influxPort"].as<uint16_t>());
    }
    if (doc["influxToken"].is<const char*>()) {
        Config::setInfluxToken(doc["influxToken"].as<const char*>());
    }
    if (doc["influxBucket"].is<const char*>()) {
        Config::setInfluxBucket(doc["influxBucket"].as<const char*>());
    }
    if (doc["influxOrg"].is<const char*>()) {
        Config::setInfluxOrg(doc["influxOrg"].as<const char*>());
    }
    if (doc["influxPrefix"].is<const char*>()) {
        Config::setInfluxPrefix(doc["influxPrefix"].as<const char*>());
    }

    doc.clear();

    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}
