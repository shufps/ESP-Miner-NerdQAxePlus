#include "handler_can_swarm.h"

#include <stdio.h>
#include <string.h>
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_app_desc.h"

#include "ArduinoJson.h"
#include "http_cors.h"
#include "http_utils.h"
#include "psram_allocator.h"
#include "global_state.h"
#include "nvs_config.h"
#include "tasks/can_master_task.h"
#include "tasks/can_sender.h"

static const char *TAG = "http_can_swarm";

esp_err_t GET_can_nodes(httpd_req_t *req)
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

    Board *board       = SYSTEM_MODULE.getBoard();
    bool   shutdown    = POWER_MANAGEMENT_MODULE.isShutdown();
    int    numFans     = board->getNumFans();

    float masterHashRate = !shutdown ? SYSTEM_MODULE.getCurrentHashrate() : 0.0f;
    float masterPower    = POWER_MANAGEMENT_MODULE.getPower();

    // Fleet totals: master + all active slaves
    float slaveHashRate = 0.0f;
    for (int i = 1; i < CAN_SLAVE_MAX; i++) {
        if (!can_master_is_slave_known((uint8_t) i)) continue;
        can_slave_telemetry_t st = {};
        if (can_master_get_slave_telemetry((uint8_t) i, &st) && !st.shutdown) {
            slaveHashRate += st.hashRate;
        }
    }
    float fleetHashRate = masterHashRate + slaveHashRate;

    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);

    // --- Fleet totals ---
    JsonObject fleet = doc["fleet"].to<JsonObject>();
    fleet["hashRate"] = fleetHashRate;
    fleet["power"]    = masterPower + can_master_get_slave_fleet_power();

    JsonArray arr = doc["nodes"].to<JsonArray>();

    // --- Node [0]: master board ---
    {
        JsonObject master = arr.add<JsonObject>();
        master["id"]       = 0;
        master["mac"]      = SYSTEM_MODULE.getMacAddress();
        master["active"]   = true;
        master["foreign"]  = false;
        master["isMaster"] = true;

        master["deviceModel"]       = board->getDeviceModel();
        master["version"]           = esp_app_get_description()->version;
        master["hashRate"]          = masterHashRate;
        master["temp"]              = POWER_MANAGEMENT_MODULE.getChipTempMax();
        master["vrTemp"]            = POWER_MANAGEMENT_MODULE.getVRTemp();
        master["power"]             = POWER_MANAGEMENT_MODULE.getPower();
        master["current"]           = (int) POWER_MANAGEMENT_MODULE.getCurrent();
        master["coreVoltageActual"] = (int) (board->getVout() * 1000.0f);
        master["fanRpm"]            = POWER_MANAGEMENT_MODULE.getFanRPM(0);
        master["fanRpm2"]           = numFans > 1 ? POWER_MANAGEMENT_MODULE.getFanRPM(1) : 0;
        master["fanSpeed"]          = POWER_MANAGEMENT_MODULE.getFanPerc(0);
        master["fanSpeed2"]         = numFans > 1 ? POWER_MANAGEMENT_MODULE.getFanPerc(1) : 0;
        master["shutdown"]          = shutdown;
        master["boardError"]        = (int) SYSTEM_MODULE.getBoardError();
        master["freeHeapInt"]       = (int) heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        master["frequency"]         = board->getAsicFrequency();
        master["coreVoltage"]       = board->getAsicVoltageMillis();
        master["flipScreen"]        = board->isFlipScreenEnabled();
        master["autoScreenOff"]     = Config::isAutoScreenOffEnabled();

        {
            JsonArray asicTemps = master["asicTemps"].to<JsonArray>();
            for (int j = 0; j < board->getAsicCount(); j++) {
                asicTemps.add(board->getChipTemp(j));
            }
        }

        {
            JsonArray fans = master["fans"].to<JsonArray>();
            for (int ch = 0; ch < numFans; ch++) {
                JsonObject fan   = fans.add<JsonObject>();
                fan["mode"]        = Config::getFanMode(ch);
                fan["manualSpeed"] = Config::getFanManualSpeed(ch);
                fan["overheatTemp"]= Config::getFanOverheatTemp(ch);
                fan["targetTemp"]  = board->getPidSettings(ch)->targetTemp;
                fan["rpm"]         = POWER_MANAGEMENT_MODULE.getFanRPM(ch);
                fan["speedPerc"]   = POWER_MANAGEMENT_MODULE.getFanPerc(ch);
            }
        }
    }

    // --- Nodes [1..N]: CAN slaves ---
    for (int i = 1; i < CAN_SLAVE_MAX; i++) {
        if (!can_master_is_slave_known((uint8_t) i)) continue;

        uint8_t mac[6];
        can_master_get_slave_mac((uint8_t) i, mac);

        can_slave_telemetry_t t   = {};
        can_slave_config_t    cfg = {};
        bool has_telem  = can_master_get_slave_telemetry((uint8_t) i, &t);
        bool has_config = can_master_get_slave_config((uint8_t) i, &cfg);

        char device_model[sizeof(cfg.deviceModel) + 1] = {};
        char fw_version[sizeof(cfg.fwVersion)     + 1] = {};
        if (has_config) {
            memcpy(device_model, cfg.deviceModel, sizeof(cfg.deviceModel));
            memcpy(fw_version,   cfg.fwVersion,   sizeof(cfg.fwVersion));
        }

        char mac_str[18];
        snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

        JsonObject node = arr.add<JsonObject>();
        node["id"]      = i;
        node["mac"]     = mac_str;
        node["active"]  = can_master_is_slave_active((uint8_t) i);
        node["foreign"] = can_master_is_slave_foreign((uint8_t) i);

        node["deviceModel"]        = device_model;
        node["version"]            = fw_version;
        node["hashRate"]           = has_telem ? t.hashRate          : 0.0f;
        node["temp"]               = has_telem ? t.temp              : 0.0f;
        node["vrTemp"]             = has_telem ? t.vrTemp            : 0.0f;
        node["power"]              = has_telem ? t.power             : 0.0f;
        node["current"]            = has_telem ? t.current           : 0;
        node["coreVoltageActual"]  = has_telem ? t.coreVoltageActual : 0;
        node["fanRpm"]             = has_telem ? t.fanRpm            : 0;
        node["fanRpm2"]            = has_telem ? t.fanRpm2           : 0;
        node["fanSpeed"]           = has_telem ? t.fanSpeed          : 0;
        node["fanSpeed2"]          = has_telem ? t.fanSpeed2         : 0;
        node["shutdown"]           = has_telem && t.shutdown;
        node["boardError"]         = has_telem ? t.boardError        : 0;
        node["freeHeapInt"]        = has_telem ? t.freeHeap          : 0;
        node["frequency"]          = has_config ? cfg.freqMhz        : 0;
        node["coreVoltage"]        = has_config ? cfg.voltageMv      : 0;
        node["flipScreen"]         = has_config && cfg.flipScreen;
        node["autoScreenOff"]      = has_config && cfg.autoScreenOff;

        {
            JsonArray asicTemps = node["asicTemps"].to<JsonArray>();
            for (int j = 0; j < 4; j++) {
                asicTemps.add(has_telem ? t.asicTemps[j] : 0.0f);
            }
        }

        {
            JsonArray fans = node["fans"].to<JsonArray>();
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

    // Extract id from URI: /api/v2/can/nodes/{id}
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

    if (validateOTP(req) != ESP_OK) {
        return ESP_FAIL;
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

    if (doc["flipScreen"].is<bool>() || doc["autoScreenOff"].is<bool>()) {
        uint8_t flip    = doc["flipScreen"].as<bool>()    ? 1 : 0;
        uint8_t autoOff = doc["autoScreenOff"].as<bool>() ? 1 : 0;
        can_master_set_slave_display(slave_id, flip, autoOff);
    }

    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

static esp_err_t parse_slave_id(httpd_req_t *req, uint8_t *out_id)
{
    const char *uri = req->uri;
    // URI format: /api/v2/can/nodes/{id}/action — find second-to-last slash
    const char *last = strrchr(uri, '/');
    if (!last || last == uri) return ESP_FAIL;
    const char *prev = last - 1;
    while (prev > uri && *prev != '/') prev--;
    if (*prev != '/') return ESP_FAIL;
    int id = atoi(prev + 1);
    if (id < 1 || id >= CAN_SLAVE_MAX) return ESP_FAIL;
    *out_id = (uint8_t) id;
    return ESP_OK;
}

// Single POST handler for /api/v2/can/nodes/* — dispatches by trailing action.
// ESP-IDF httpd only supports wildcards at the end of the URI, so we can't
// register separate handlers for /nodes/*/restart, /nodes/*/shutdown, etc.
esp_err_t POST_can_slave_action(httpd_req_t *req)
{
    ConGuard g(http_server, req);
    if (is_network_allowed(req) != ESP_OK)
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    if (set_cors_headers(req) != ESP_OK) { httpd_resp_send_500(req); return ESP_FAIL; }

    uint8_t slave_id;
    if (parse_slave_id(req, &slave_id) != ESP_OK)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid id");

    // Dispatch based on the last path segment (restart, shutdown, identify)
    const char *last = strrchr(req->uri, '/');
    if (!last) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid uri");
    last++; // skip the '/'

    if (strcmp(last, "restart") == 0) {
        can_master_restart_slave(slave_id);
    } else if (strcmp(last, "shutdown") == 0) {
        can_master_shutdown_slave(slave_id);
    } else if (strcmp(last, "identify") == 0) {
        can_master_identify_slave(slave_id);
    } else {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "unknown action");
    }

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

    if (validateOTP(req) != ESP_OK) {
        return ESP_FAIL;
    }

    // Extract id from URI: /api/v2/can/nodes/{id}
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
