#include "handler_can_swarm.h"

#include <stdio.h>
#include <string.h>
#include "esp_http_server.h"
#include "esp_log.h"

#include "ArduinoJson.h"
#include "http_cors.h"
#include "http_utils.h"
#include "psram_allocator.h"
#include "tasks/can_master_task.h"
#include "tasks/can_sender.h"

static const char *TAG = "http_can_swarm";

esp_err_t GET_can_slaves(httpd_req_t *req)
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

    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);
    JsonArray arr = doc.to<JsonArray>();

    for (int i = 1; i < CAN_SLAVE_MAX; i++) {
        if (!can_master_is_slave_known((uint8_t) i)) continue;

        uint8_t mac[6];
        can_master_get_slave_mac((uint8_t) i, mac);

        can_slave_telemetry_t t   = {};
        can_slave_config_t    cfg = {};
        bool has_telem  = can_master_get_slave_telemetry((uint8_t) i, &t);
        bool has_config = can_master_get_slave_config((uint8_t) i, &cfg);

        // Null-terminate strings from config struct
        char device_model[sizeof(cfg.deviceModel) + 1] = {};
        char fw_version[sizeof(cfg.fwVersion)     + 1] = {};
        if (has_config) {
            memcpy(device_model, cfg.deviceModel, sizeof(cfg.deviceModel));
            memcpy(fw_version,   cfg.fwVersion,   sizeof(cfg.fwVersion));
        }

        char mac_str[18];
        snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

        JsonObject slave = arr.add<JsonObject>();
        slave["id"]      = i;
        slave["mac"]     = mac_str;
        slave["active"]  = can_master_is_slave_active((uint8_t) i);
        slave["foreign"] = can_master_is_slave_foreign((uint8_t) i);

        // --- fields matching info endpoint ---
        slave["deviceModel"]        = device_model;
        slave["version"]            = fw_version;           // info: "version"
        slave["hashRate"]           = has_telem ? t.hashRate          : 0.0f;
        slave["temp"]               = has_telem ? t.temp              : 0.0f;
        slave["vrTemp"]             = has_telem ? t.vrTemp            : 0.0f;
        slave["power"]              = has_telem ? t.power             : 0.0f;
        slave["current"]            = has_telem ? t.current           : 0;
        slave["coreVoltageActual"]  = has_telem ? t.coreVoltageActual : 0;
        slave["fanrpm"]             = has_telem ? t.fanRpm            : 0;   // info: "fanrpm"
        slave["fanrpm2"]            = has_telem ? t.fanRpm2           : 0;   // info: "fanrpm2"
        slave["fanspeed"]           = has_telem ? t.fanSpeed          : 0;   // info: "fanspeed"
        slave["fanspeed2"]          = has_telem ? t.fanSpeed2         : 0;   // info: "fanspeed2"
        slave["shutdown"]           = has_telem && t.shutdown;
        slave["boardError"]         = has_telem ? t.boardError        : 0;
        slave["frequency"]          = has_config ? cfg.freqMhz        : 0;   // info: "frequency"
        slave["coreVoltage"]        = has_config ? cfg.voltageMv      : 0;   // info: "coreVoltage"
        slave["flipscreen"]         = has_config && cfg.flipScreen;           // info: "flipscreen"
        slave["autoscreenoff"]      = has_config && cfg.autoScreenOff;        // info: "autoscreenoff"

        {
            JsonArray asic_temps = slave["asicTemps"].to<JsonArray>();
            for (int j = 0; j < 4; j++) {
                asic_temps.add(has_telem ? t.asicTemps[j] : 0.0f);
            }
        }

        // fans array — matches info endpoint structure (minus pid, not available from slave)
        {
            JsonArray fans = slave["fans"].to<JsonArray>();
            for (int ch = 0; ch < 2; ch++) {
                JsonObject fan      = fans.add<JsonObject>();
                fan["mode"]         = has_config ? (ch == 0 ? cfg.fan0Mode       : cfg.fan1Mode)       : 0;
                fan["manualSpeed"]  = has_config ? (ch == 0 ? cfg.fan0Speed      : cfg.fan1Speed)      : 0;
                fan["overheatTemp"] = has_config ? (ch == 0 ? cfg.fan0Overheat   : cfg.fan1Overheat)   : 0;
                fan["targetTemp"]   = has_config ? (ch == 0 ? cfg.fan0TargetTemp : cfg.fan1TargetTemp) : 0;
                fan["rpm"]          = has_telem  ? (ch == 0 ? t.fanRpm           : t.fanRpm2)          : 0;
                fan["speedPerc"]    = has_telem  ? (ch == 0 ? t.fanSpeed         : t.fanSpeed2)        : 0;
            }
        }
    }

    return sendJsonResponse(req, doc);
}

esp_err_t PATCH_can_slave(httpd_req_t *req)
{
    ConGuard g(http_server, req);

    if (set_cors_headers(req) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Extract id from URI: /api/can/slaves/{id}
    const char *uri = req->uri;
    const char *last_slash = strrchr(uri, '/');
    if (!last_slash || *(last_slash + 1) == '\0') {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing id");
    }
    int id = atoi(last_slash + 1);
    if (id < 1 || id >= CAN_SLAVE_MAX) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid id");
    }
    uint8_t slave_id = (uint8_t) id;

    char body[256];
    int received = httpd_req_recv(req, body, sizeof(body) - 1);
    if (received <= 0) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "no body");
    }
    body[received] = '\0';

    JsonDocument doc;
    if (deserializeJson(doc, body) != DeserializationError::Ok) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid json");
    }

    // field names matching info endpoint PATCH
    if (doc["frequency"].is<uint16_t>())
        can_master_set_slave_freq(slave_id, doc["frequency"].as<uint16_t>());

    if (doc["coreVoltage"].is<uint16_t>())
        can_master_set_slave_voltage(slave_id, doc["coreVoltage"].as<uint16_t>());

    // fans array — matches info endpoint PATCH structure
    if (doc["fans"].is<JsonArray>()) {
        JsonArray fans = doc["fans"].as<JsonArray>();
        int ch = 0;
        for (JsonObject fan : fans) {
            if (ch > 1) break;
            uint8_t mode       = fan["mode"].as<uint8_t>();
            uint8_t speed      = fan["manualSpeed"].as<uint8_t>();
            uint8_t targetTemp = fan["targetTemp"].as<uint8_t>();
            uint8_t overheat   = fan["overheatTemp"].as<uint8_t>();
            can_master_set_slave_fan(slave_id, (uint8_t) ch, mode, speed, targetTemp, overheat);
            ch++;
        }
    }

    if (doc["flipscreen"].is<bool>() || doc["autoscreenoff"].is<bool>()) {
        uint8_t flip    = doc["flipscreen"].as<bool>()    ? 1 : 0;
        uint8_t autoOff = doc["autoscreenoff"].as<bool>() ? 1 : 0;
        can_master_set_slave_display(slave_id, flip, autoOff);
    }

    if (doc["restart"].is<bool>() && doc["restart"].as<bool>())
        can_master_restart_slave(slave_id);

    if (doc["shutdown"].is<bool>() && doc["shutdown"].as<bool>())
        can_master_shutdown_slave(slave_id);

    if (doc["identify"].is<bool>() && doc["identify"].as<bool>())
        can_master_identify_slave(slave_id);

    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

esp_err_t DELETE_can_slave(httpd_req_t *req)
{
    ConGuard g(http_server, req);

    if (set_cors_headers(req) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Extract id from URI: /api/can/slaves/{id}
    const char *uri = req->uri;
    const char *last_slash = strrchr(uri, '/');
    if (!last_slash || *(last_slash + 1) == '\0') {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing id");
    }

    int id = atoi(last_slash + 1);
    if (id < 1 || id >= CAN_SLAVE_MAX) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid id");
    }

    can_master_delete_slave((uint8_t) id);
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}
