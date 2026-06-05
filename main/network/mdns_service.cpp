#include "mdns_service.h"

#include <stdio.h>
#include <string.h>

#include "esp_app_desc.h"
#include "esp_log.h"
#include "mdns.h"

static const char *TAG = "mdns_svc";
static bool s_started = false;

extern "C" void mdns_service_start(const char *hostname,
                                   const char *device_model,
                                   const char *asic_model,
                                   int asic_count,
                                   int board_version)
{
    if (s_started) {
        return;
    }

    esp_err_t err = mdns_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mdns_init failed: %s", esp_err_to_name(err));
        return;
    }

    const char *host = (hostname && hostname[0]) ? hostname : "nerdqaxe";
    mdns_hostname_set(host);
    mdns_instance_name_set(host);

    char board_str[8];
    snprintf(board_str, sizeof(board_str), "%d", board_version);
    char asic_count_str[8];
    snprintf(asic_count_str, sizeof(asic_count_str), "%d", asic_count);
    const esp_app_desc_t *app = esp_app_get_description();

    mdns_txt_item_t txt[] = {
        {"board",      board_str},
        {"family",     device_model ? device_model : "NerdQAxe"},
        {"asic",       asic_model   ? asic_model   : ""},
        {"asic_count", asic_count_str},
        {"fw_version", app ? app->version : ""},
    };
    err = mdns_service_add(host, "_http", "_tcp", 80, txt, sizeof(txt) / sizeof(txt[0]));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "mdns_service_add failed: %s", esp_err_to_name(err));
        return;
    }

    err = mdns_service_subtype_add_for_host(host, "_http", "_tcp", NULL, "_axeos");
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "mdns_service_subtype_add failed: %s", esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "mDNS up: %s.local _http._tcp:80 subtype=_axeos "
                 "(board=%s family=%s asic=%s count=%s fw=%s)",
             host, board_str,
             device_model ? device_model : "",
             asic_model   ? asic_model   : "",
             asic_count_str,
             app ? app->version : "");
    s_started = true;
}
