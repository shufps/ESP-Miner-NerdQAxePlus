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
    // close connection when out of scope
    ConGuard g(http_server, req);

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
    doc["url"]    = influxURL;
    doc["port"]   = Config::getInfluxPort();
    doc["bucket"] = influxBucket;
    doc["org"]    = influxOrg;
    doc["prefix"] = influxPrefix;
    doc["enabled"] = Config::isInfluxEnabled() ? 1 : 0;

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

    if (validateOTP(req) != ESP_OK) {
        return ESP_FAIL;
    }

    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);

    esp_err_t err = getJsonData(req, doc);
    if (err != ESP_OK) {
        return err;
    }

    // Check and apply each setting if the key exists and has the correct type
    if (doc["enabled"].is<bool>()) {
        Config::setInfluxEnabled(doc["enabled"].as<bool>());
    }
    if (doc["url"].is<const char*>()) {
        Config::setInfluxURL(doc["url"].as<const char*>());
    }
    if (doc["port"].is<uint16_t>()) {
        Config::setInfluxPort(doc["port"].as<uint16_t>());
    }
    if (doc["token"].is<const char*>()) {
        Config::setInfluxToken(doc["token"].as<const char*>());
    }
    if (doc["bucket"].is<const char*>()) {
        Config::setInfluxBucket(doc["bucket"].as<const char*>());
    }
    if (doc["org"].is<const char*>()) {
        Config::setInfluxOrg(doc["org"].as<const char*>());
    }
    if (doc["prefix"].is<const char*>()) {
        Config::setInfluxPrefix(doc["prefix"].as<const char*>());
    }

    doc.clear();

    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}
