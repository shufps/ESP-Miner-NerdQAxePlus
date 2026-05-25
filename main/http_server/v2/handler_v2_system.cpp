#include "handler_v2_system.h"

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_app_desc.h"

#include "ArduinoJson.h"
#include "psram_allocator.h"
#include "global_state.h"
#include "nvs_config.h"
#include "http_cors.h"
#include "http_utils.h"

static const char *TAG = "http_v2_system";

esp_err_t GET_V2_system(httpd_req_t *req)
{
    ConGuard g(http_server, req);

    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    httpd_resp_set_type(req, "application/json");
    if (set_cors_headers(req) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    Board *board = SYSTEM_MODULE.getBoard();

    char *hostname = Config::getHostname();
    char *ssid     = Config::getWifiSSID();

    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);

    // Device identity
    doc["deviceModel"] = board->getDeviceModel();
    doc["asicModel"]   = board->getAsicModel();
    doc["version"]     = esp_app_get_description()->version;

    // Uptime & reset
    doc["uptimeSeconds"]    = (esp_timer_get_time() - SYSTEM_MODULE.getStartTime()) / 1000000;
    doc["lastResetReason"]  = SYSTEM_MODULE.getLastResetReason();

    // Network
    JsonObject network = doc["network"].to<JsonObject>();
    network["hostname"]   = hostname;
    network["ssid"]       = ssid;
    network["macAddr"]    = SYSTEM_MODULE.getMacAddress();
    network["ipAddr"]     = SYSTEM_MODULE.getIPAddress();
    network["wifiStatus"] = SYSTEM_MODULE.getWifiStatus();
    network["wifiRSSI"]   = SYSTEM_MODULE.get_wifi_rssi();

    // Memory
    JsonObject memory = doc["memory"].to<JsonObject>();
    memory["freeHeap"]    = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    memory["freeHeapInt"] = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);

    free(hostname);
    free(ssid);

    return sendJsonResponse(req, doc);
}
