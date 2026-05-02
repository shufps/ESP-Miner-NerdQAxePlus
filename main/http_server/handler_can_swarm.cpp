#include "handler_can_swarm.h"

#include <stdio.h>
#include <string.h>
#include "esp_http_server.h"
#include "esp_log.h"

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

        char buf[512];
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
            "\"shutdown\":%s"
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
            has_telem ? t.fanRpm   : 0,
            has_telem ? t.fanRpm2  : 0,
            has_telem ? t.fanSpeed : 0,
            has_telem ? t.fanSpeed2: 0,
            has_telem ? t.power    : 0.0f,
            has_telem ? t.current  : 0,
            has_telem ? t.coreVoltageActual : 0,
            (has_telem && t.shutdown) ? "true" : "false"
        );

        httpd_resp_sendstr_chunk(req, buf);
        first = false;
    }

    httpd_resp_sendstr_chunk(req, "]");
    httpd_resp_sendstr_chunk(req, NULL);
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
