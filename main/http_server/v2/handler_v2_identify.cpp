#include "handler_v2_identify.h"

#include "esp_http_server.h"
#include "esp_log.h"

#include "ArduinoJson.h"
#include "psram_allocator.h"
#include "global_state.h"
#include "nvs_config.h"
#include "http_cors.h"
#include "http_utils.h"

static const char *TAG = "http_v2_identify";

esp_err_t GET_V2_identify(httpd_req_t *req)
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

    Board* board = SYSTEM_MODULE.getBoard();

    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);

    doc["deviceModel"] = board->getDeviceModel();
    doc["defaultTheme"] = board->getDefaultTheme();
    doc["otp"]          = Config::isOTPEnabled();

    JsonObject can = doc["can"].to<JsonObject>();
    can["enabled"] = Config::isCanEnabled();

    return sendJsonResponse(req, doc);
}
