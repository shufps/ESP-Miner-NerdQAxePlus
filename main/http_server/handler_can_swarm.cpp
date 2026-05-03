#include "handler_can_swarm.h"

#include <stdio.h>
#include <string.h>
#include "esp_http_server.h"
#include "esp_log.h"

#include "ArduinoJson.h"
#include "http_cors.h"
#include "http_utils.h"
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

    httpd_resp_sendstr_chunk(req, "[");
    bool first = true;

    for (int i = 1; i < CAN_SLAVE_MAX; i++) {
        if (!can_master_is_slave_known((uint8_t) i)) continue;

        uint8_t mac[6];
        can_master_get_slave_mac((uint8_t) i, mac);

        can_slave_telemetry_t t = {};
        bool has_telem = can_master_get_slave_telemetry((uint8_t) i, &t);
        bool active    = can_master_is_slave_active((uint8_t) i);
        bool foreign   = can_master_is_slave_foreign((uint8_t) i);

        char buf[768];
        snprintf(buf, sizeof(buf),
            "%s{"
            "\"id\":%d,"
            "\"mac\":\"%02X:%02X:%02X:%02X:%02X:%02X\","
            "\"active\":%s,"
            "\"foreign\":%s,"
            "\"hashRate\":%.2f,"
            "\"temp\":%.1f,"
            "\"vrTemp\":%.1f,"
            "\"asicTemps\":[%.1f,%.1f,%.1f,%.1f],"
            "\"fanRpm\":%u,"
            "\"fanRpm2\":%u,"
            "\"fanSpeed\":%u,"
            "\"fanSpeed2\":%u,"
            "\"power\":%.1f,"
            "\"current\":%u,"
            "\"coreVoltageActual\":%u,"
            "\"shutdown\":%s,"
            "\"boardError\":%u,"
            "\"freqMhz\":%u,"
            "\"voltageMv\":%u,"
            "\"fan0Mode\":%u,\"fan0Speed\":%u,\"fan0TargetTemp\":%u,\"fan0Overheat\":%u,"
            "\"fan1Mode\":%u,\"fan1Speed\":%u,\"fan1TargetTemp\":%u,\"fan1Overheat\":%u,"
            "\"flipScreen\":%s,"
            "\"autoScreenOff\":%s"
            "}",
            first ? "" : ",",
            i,
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
            active  ? "true" : "false",
            foreign ? "true" : "false",
            has_telem ? t.hashRate : 0.0f,
            has_telem ? t.temp     : 0.0f,
            has_telem ? t.vrTemp   : 0.0f,
            has_telem ? t.asicTemps[0] : 0.0f,
            has_telem ? t.asicTemps[1] : 0.0f,
            has_telem ? t.asicTemps[2] : 0.0f,
            has_telem ? t.asicTemps[3] : 0.0f,
            has_telem ? t.fanRpm    : 0,
            has_telem ? t.fanRpm2   : 0,
            has_telem ? t.fanSpeed  : 0,
            has_telem ? t.fanSpeed2 : 0,
            has_telem ? t.power     : 0.0f,
            has_telem ? t.current   : 0,
            has_telem ? t.coreVoltageActual : 0,
            (has_telem && t.shutdown)    ? "true" : "false",
            has_telem ? t.boardError     : 0,
            has_telem ? t.freqMhz        : 0,
            has_telem ? t.voltageMv      : 0,
            has_telem ? t.fan0Mode       : 0,
            has_telem ? t.fan0Speed      : 0,
            has_telem ? t.fan0TargetTemp : 0,
            has_telem ? t.fan0Overheat   : 0,
            has_telem ? t.fan1Mode       : 0,
            has_telem ? t.fan1Speed      : 0,
            has_telem ? t.fan1TargetTemp : 0,
            has_telem ? t.fan1Overheat   : 0,
            (has_telem && t.flipScreen)    ? "true" : "false",
            (has_telem && t.autoScreenOff) ? "true" : "false"
        );

        httpd_resp_sendstr_chunk(req, buf);
        first = false;
    }

    httpd_resp_sendstr_chunk(req, "]");
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
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

    // Read body
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

    if (doc["freqMhz"].is<uint16_t>())
        can_master_set_slave_freq(slave_id, doc["freqMhz"].as<uint16_t>());

    if (doc["voltageMv"].is<uint16_t>())
        can_master_set_slave_voltage(slave_id, doc["voltageMv"].as<uint16_t>());

    for (uint8_t ch = 0; ch < 2; ch++) {
        const char *key = (ch == 0) ? "fan0" : "fan1";
        if (doc[key].is<JsonObject>()) {
            JsonObject fan = doc[key].as<JsonObject>();
            can_master_set_slave_fan(slave_id, ch,
                fan["mode"].as<uint8_t>(),
                fan["speed"].as<uint8_t>(),
                fan["targetTemp"].as<uint8_t>(),
                fan["overheat"].as<uint8_t>());
        }
    }

    if (doc["flipScreen"].is<bool>() || doc["autoScreenOff"].is<bool>()) {
        uint8_t flip    = doc["flipScreen"].as<bool>()    ? 1 : 0;
        uint8_t autoOff = doc["autoScreenOff"].as<bool>() ? 1 : 0;
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
