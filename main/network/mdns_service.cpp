#include "mdns_service.h"

#include <stdio.h>
#include "esp_app_desc.h"
#include "esp_log.h"
#include "mdns.h"

static const char *TAG = "mdns_svc";
static bool s_started = false;

void mdns_service_start(const char *hostname, Board *board)
{
    if (s_started) return;

    esp_err_t err = mdns_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mdns_init failed: %s", esp_err_to_name(err));
        return;
    }

    const char *host = (hostname && hostname[0]) ? hostname : "nerdminer";
    mdns_hostname_set(host);
    mdns_instance_name_set(host);

    char board_ver[8];
    snprintf(board_ver, sizeof(board_ver), "%d", board ? board->getVersion() : 0);
    char asic_count[8];
    snprintf(asic_count, sizeof(asic_count), "%d", board ? board->getAsicCount() : 0);
    const esp_app_desc_t *app = esp_app_get_description();

    mdns_txt_item_t txt[] = {
        {"board",      board_ver},
        {"family",     board ? board->getDeviceModel() : "unknown"},
        {"asic",       board ? board->getAsicModel() : ""},
        {"asic_count", asic_count},
        {"fw_version", app ? app->version : ""},
    };

    err = mdns_service_add(host, "_http", "_tcp", 80, txt, sizeof(txt) / sizeof(txt[0]));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "mdns_service_add failed: %s", esp_err_to_name(err));
        return;
    }

    err = mdns_service_subtype_add_for_host(host, "_http", "_tcp", NULL, "_axeos");
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "subtype add failed: %s", esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "mDNS up: %s.local (family=%s, asic=%s, count=%s, fw=%s)",
             host,
             board ? board->getDeviceModel() : "",
             board ? board->getAsicModel() : "",
             asic_count,
             app ? app->version : "");
    s_started = true;
}
