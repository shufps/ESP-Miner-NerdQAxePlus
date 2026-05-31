#include "handler_wifi_scan.h"

#include "esp_http_server.h"
#include "esp_log.h"

#include "ArduinoJson.h"

#include "connect.h"
#include "global_state.h"
#include "http_cors.h"
#include "http_utils.h"
#include "psram_allocator.h"
#include "network_manager.h"

static const char *TAG = "http_wifi_scan";

esp_err_t GET_wifi_scan(httpd_req_t *req)
{
    ConGuard g(http_server, req);

    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    if (!NETWORK.isApActive()) {
        return httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "WiFi scan only available while AP is active");
    }

    httpd_resp_set_type(req, "application/json");
    if (set_cors_headers(req) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    wifi_ap_record_simple_t ap_records[WIFI_SCAN_MAX_APS];
    uint16_t ap_count = 0;

    esp_err_t err = wifi_scan(ap_records, &ap_count);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "wifi_scan failed: %s", esp_err_to_name(err));
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "WiFi scan failed");
    }

    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);
    JsonArray networks = doc["networks"].to<JsonArray>();

    for (uint16_t i = 0; i < ap_count; i++) {
        JsonObject n = networks.add<JsonObject>();
        n["ssid"]     = (const char *) ap_records[i].ssid;
        n["rssi"]     = ap_records[i].rssi;
        n["authmode"] = (int) ap_records[i].authmode;
    }

    return sendJsonResponse(req, doc);
}
