#include <string.h>
#include "esp_log.h"
#include "esp_heap_caps.h" 
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs_config.h"

#define NVS_CONFIG_NAMESPACE "main"
#define MAX_CONFIG_ENTRIES 96

#define CONFIG_LOCK()   xSemaphoreTake(config_mutex, portMAX_DELAY)
#define CONFIG_UNLOCK() xSemaphoreGive(config_mutex)

namespace Config {

static const char *TAG = "nvs_config";

// --- Config cache structure ---
typedef enum {
    CONFIG_TYPE_STRING,
    CONFIG_TYPE_U16,
    CONFIG_TYPE_U64
} config_type_t;

typedef struct {
    const char* key;
    config_type_t type;
    bool valid;
    bool dirty;
    union {
        char* str_value;
        uint16_t u16_value;
        uint64_t u64_value;
    } data;
} ConfigCacheEntry;

static ConfigCacheEntry* config_cache = nullptr;
static SemaphoreHandle_t config_mutex = nullptr;

// --- Initialize cache and mutex ---
void init_cache() {
    if (!config_cache) {
        config_cache = (ConfigCacheEntry*) heap_caps_calloc(MAX_CONFIG_ENTRIES, sizeof(ConfigCacheEntry), MALLOC_CAP_SPIRAM);
        ESP_LOGE(TAG, "Failed to allocate config cache in PSRAM");
    }
    if (!config_mutex) {
        config_mutex = xSemaphoreCreateMutex();
    }
}

// --- Utility to find or create cache entry ---
static ConfigCacheEntry* get_cache_entry(const char* key, config_type_t type) {
    CONFIG_LOCK();
    for (int i = 0; i < MAX_CONFIG_ENTRIES; ++i) {
        if (config_cache[i].key && strcmp(config_cache[i].key, key) == 0) {
            CONFIG_UNLOCK();
            return &config_cache[i];
        }
    }
    for (int i = 0; i < MAX_CONFIG_ENTRIES; ++i) {
        if (!config_cache[i].key) {
            config_cache[i].key = key;
            config_cache[i].type = type;
            config_cache[i].valid = false;
            config_cache[i].dirty = false;
            CONFIG_UNLOCK();
            return &config_cache[i];
        }
    }
    ESP_LOGW(TAG, "Config cache full, could not store key: %s", key);
    CONFIG_UNLOCK();
    return NULL;
}

char *nvs_config_get_string(const char *key, const char *default_value) {
    ConfigCacheEntry* entry = get_cache_entry(key, CONFIG_TYPE_STRING);
    if (!entry) return strdup(default_value);

    CONFIG_LOCK();
    if (entry->valid) {
        ESP_LOGD(TAG, "Cache hit: %s", key);
        char* result = strdup(entry->data.str_value);
        CONFIG_UNLOCK();
        return result;
    }
    CONFIG_UNLOCK();

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

    char *val = (char *) malloc(size);
    err = nvs_get_str(handle, key, val, &size);
    nvs_close(handle);

    if (err != ESP_OK) {
        free(val);
        return strdup(default_value);
    }

    CONFIG_LOCK();
    entry->data.str_value = strdup(val);
    entry->valid = true;
    CONFIG_UNLOCK();
    free(val);
    return strdup(entry->data.str_value);
}

void nvs_config_set_string(const char *key, const char *value) {
    ConfigCacheEntry* entry = get_cache_entry(key, CONFIG_TYPE_STRING);
    if (!entry) return;

    CONFIG_LOCK();
    if (entry->valid && entry->data.str_value) {
        free(entry->data.str_value);
    }
    entry->data.str_value = strdup(value);
    entry->valid = true;
    entry->dirty = true;
    CONFIG_UNLOCK();
}

uint16_t nvs_config_get_u16(const char *key, const uint16_t default_value) {
    ConfigCacheEntry* entry = get_cache_entry(key, CONFIG_TYPE_U16);
    if (!entry) return default_value;

    CONFIG_LOCK();
    if (entry->valid) {
        ESP_LOGD(TAG, "Cache hit: %s (u16)", key);
        uint16_t val = entry->data.u16_value;
        CONFIG_UNLOCK();
        return val;
    }
    CONFIG_UNLOCK();

    nvs_handle handle;
    esp_err_t err;
    err = nvs_open(NVS_CONFIG_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return default_value;
    }

    uint16_t val;
    err = nvs_get_u16(handle, key, &val);
    nvs_close(handle);

    if (err != ESP_OK) {
        return default_value;
    }

    CONFIG_LOCK();
    entry->data.u16_value = val;
    entry->valid = true;
    CONFIG_UNLOCK();
    return val;
}

void nvs_config_set_u16(const char *key, const uint16_t value) {
    ConfigCacheEntry* entry = get_cache_entry(key, CONFIG_TYPE_U16);
    if (!entry) return;

    CONFIG_LOCK();
    entry->data.u16_value = value;
    entry->valid = true;
    entry->dirty = true;
    CONFIG_UNLOCK();
}

uint64_t nvs_config_get_u64(const char *key, const uint64_t default_value) {
    ConfigCacheEntry* entry = get_cache_entry(key, CONFIG_TYPE_U64);
    if (!entry) return default_value;

    CONFIG_LOCK();
    if (entry->valid) {
        ESP_LOGD(TAG, "Cache hit: %s (u64)", key);
        uint64_t val = entry->data.u64_value;
        CONFIG_UNLOCK();
        return val;
    }
    CONFIG_UNLOCK();

    nvs_handle handle;
    esp_err_t err;
    err = nvs_open(NVS_CONFIG_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return default_value;
    }

    uint64_t val;
    err = nvs_get_u64(handle, key, &val);
    nvs_close(handle);

    if (err != ESP_OK) {
        return default_value;
    }

    CONFIG_LOCK();
    entry->data.u64_value = val;
    entry->valid = true;
    CONFIG_UNLOCK();
    return val;
}

void nvs_config_set_u64(const char *key, const uint64_t value) {
    ConfigCacheEntry* entry = get_cache_entry(key, CONFIG_TYPE_U64);
    if (!entry) return;

    CONFIG_LOCK();
    entry->data.u64_value = value;
    entry->valid = true;
    entry->dirty = true;
    CONFIG_UNLOCK();
}

void flush_nvs_changes() {
    nvs_handle handle;
    esp_err_t err;
    err = nvs_open(NVS_CONFIG_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Could not open NVS for flushing");
        return;
    }

    CONFIG_LOCK();
    for (int i = 0; i < MAX_CONFIG_ENTRIES; ++i) {
        if (!config_cache[i].key || !config_cache[i].dirty) continue;

        switch (config_cache[i].type) {
            case CONFIG_TYPE_STRING:
                nvs_set_str(handle, config_cache[i].key, config_cache[i].data.str_value);
                break;
            case CONFIG_TYPE_U16:
                nvs_set_u16(handle, config_cache[i].key, config_cache[i].data.u16_value);
                break;
            case CONFIG_TYPE_U64:
                nvs_set_u64(handle, config_cache[i].key, config_cache[i].data.u64_value);
                break;
        }

        config_cache[i].dirty = false;
    }
    CONFIG_UNLOCK();

    nvs_commit(handle);
    nvs_close(handle);
    ESP_LOGI(TAG, "Configuration changes flushed to NVS");
}

bool has_dirty() {
    CONFIG_LOCK();
    for (int i = 0; i < MAX_CONFIG_ENTRIES; ++i) {
        if (config_cache[i].key && config_cache[i].dirty) {
            CONFIG_UNLOCK();
            return true;
        }
    }
    CONFIG_UNLOCK();
    return false;
}

void migrate_config() {
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
}

} // namespace Config
