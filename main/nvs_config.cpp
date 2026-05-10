#include <string.h>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_config.h"

#define NVS_CONFIG_NAMESPACE "main"

namespace Config
{

static const char *TAG = "nvs_config";

char *nvs_config_get_string(const char *key, const char *default_value)
{
    nvs_handle handle;
    esp_err_t err;
    err = nvs_open(NVS_CONFIG_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return strdup(default_value);
    }

    size_t size = 0;
    err = nvs_get_str(handle, key, NULL, &size);

    if (err != ESP_OK) {
        nvs_close(handle);
        return strdup(default_value);
    }

    char *out = (char *) malloc(size);
    err = nvs_get_str(handle, key, out, &size);

    if (err != ESP_OK) {
        free(out);
        nvs_close(handle);
        return strdup(default_value);
    }

    nvs_close(handle);
    return out;
}

void nvs_config_set_string(const char *key, const char *value)
{

    nvs_handle handle;
    esp_err_t err;
    err = nvs_open(NVS_CONFIG_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Could not open nvs");
        return;
    }

    err = nvs_set_str(handle, key, value);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Could not write nvs key: %s, value: %s", key, value);
    }

    nvs_close(handle);
}

uint16_t nvs_config_get_u16(const char *key, const uint16_t default_value)
{
    nvs_handle handle;
    esp_err_t err;
    err = nvs_open(NVS_CONFIG_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return default_value;
    }

    uint16_t out;
    err = nvs_get_u16(handle, key, &out);
    nvs_close(handle);

    if (err != ESP_OK) {
        return default_value;
    }
    return out;
}

void nvs_config_set_u16(const char *key, const uint16_t value)
{

    nvs_handle handle;
    esp_err_t err;
    err = nvs_open(NVS_CONFIG_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Could not open nvs");
        return;
    }

    err = nvs_set_u16(handle, key, value);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Could not write nvs key: %s, value: %u", key, value);
    }

    nvs_close(handle);
}

uint64_t nvs_config_get_u64(const char *key, const uint64_t default_value)
{
    nvs_handle handle;
    esp_err_t err;
    err = nvs_open(NVS_CONFIG_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return default_value;
    }

    uint64_t out;
    err = nvs_get_u64(handle, key, &out);

    if (err != ESP_OK) {
        nvs_close(handle);
        return default_value;
    }

    nvs_close(handle);
    return out;
}

void nvs_config_set_u64(const char *key, const uint64_t value)
{

    nvs_handle handle;
    esp_err_t err;
    err = nvs_open(NVS_CONFIG_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Could not open nvs");
        return;
    }

    err = nvs_set_u64(handle, key, value);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Could not write nvs key: %s, value: %llu", key, value);
    }
    nvs_close(handle);
}

bool nvs_config_has_u16(const char *key)
{
    nvs_handle handle;
    if (nvs_open(NVS_CONFIG_NAMESPACE, NVS_READONLY, &handle) != ESP_OK)
        return false;
    uint16_t dummy;
    bool exists = (nvs_get_u16(handle, key, &dummy) == ESP_OK);
    nvs_close(handle);
    return exists;
}

void migrate_config()
{
    // overwrite previously allowed 0 value to disable
    // over-temp shutdown
    uint16_t asic_overheat_temp = Config::getOverheatTemp();
    if (!asic_overheat_temp) {
        ESP_LOGW(TAG, "Overheat Temp 0 not longer allowed, setting to 70");
        setOverheatTemp(70);
    }

    // check if classic AFC is enabled and disable it
    uint16_t fan_mode = getTempControlMode();
    if (fan_mode == 1) {
        ESP_LOGW(TAG, "Disabled AFC (deprecated), Enabled manual 100%%.");
        setTempControlMode(0);
        setFanSpeed(100);
    }

    // migrate VReg overheat temp: if not yet set, inherit ASIC overheat temp
    if (!nvs_config_has_u16(NVS_CONFIG_FAN1_OVERHEAT)) {
        uint16_t asic_temp = getOverheatTemp();
        ESP_LOGI(TAG, "Migrating VReg overheat temp from ASIC value: %u°C", asic_temp);
        setFanOverheatTemp(1, asic_temp);
    }
}

} // namespace Config
