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

// Duplicates a string into specified memory (PSRAM) using heap_caps_malloc().
static char* heap_caps_strdup(const char* src, uint32_t caps) {
    if (!src) return nullptr;
    size_t len = strlen(src) + 1;

    // check for length
    constexpr size_t MAX_STR_LENGTH = 1024;
    if (len > MAX_STR_LENGTH) {
        ESP_LOGE(TAG, "String exceeds maximum allowed length of %zu bytes.", MAX_STR_LENGTH);
        return nullptr;
    }

    char* dst = (char*) heap_caps_malloc(len, caps);
    if (!dst) {
        ESP_LOGE(TAG, "Memory allocation for string duplication failed. Requested size: %zu bytes.", len);
        return nullptr;
    }
    memcpy(dst, src, len);
    return dst;
}

// --- Initialize cache and mutex ---
void init_cache() {
    if (!config_mutex) {
        config_mutex = xSemaphoreCreateMutex();
    }

    if (!config_cache) {
        config_cache = (ConfigCacheEntry*) heap_caps_calloc(MAX_CONFIG_ENTRIES, sizeof(ConfigCacheEntry), MALLOC_CAP_SPIRAM);
        if (!config_cache) {
            ESP_LOGE(TAG, "Failed to allocate config cache in PSRAM");
            // RFC: This is a fatal error, we should consider halting or restarting at this point
        }
    }
}

// --- Utility to find or create cache entry ->  This function MUST be called within a locked context ---
static ConfigCacheEntry* get_cache_entry(const char* key, config_type_t type) {
    //  Try to find an existing entry for the key
    for (int i = 0; i < MAX_CONFIG_ENTRIES; ++i) {
        if (config_cache[i].key && strcmp(config_cache[i].key, key) == 0) {
            return &config_cache[i];
        }
    }
    // If not found, find a free slot for a new entry
    for (int i = 0; i < MAX_CONFIG_ENTRIES; ++i) {
        if (!config_cache[i].key) {
            // Duplicate the key to avoid unexpected issues
            char* duplicated_key = heap_caps_strdup(key, MALLOC_CAP_SPIRAM);
            if (!duplicated_key) {
                ESP_LOGE(TAG, "Failed to duplicate key '%s' due to memory allocation issues.", key);
                return NULL;
            }
            config_cache[i].key = key;
            config_cache[i].type = type;
            config_cache[i].valid = false;
            config_cache[i].dirty = false;
            return &config_cache[i];
        }
    }
    ESP_LOGW(TAG, "Config cache is full, could not store key: %s", key);
    return NULL;
}

// --- String Functions ---
char *nvs_config_get_string(const char *key, const char *default_value) {
    CONFIG_LOCK();
    ConfigCacheEntry* entry = get_cache_entry(key, CONFIG_TYPE_STRING);
    if (!entry) {
        ESP_LOGE(TAG, "Failed to get or create cache entry for key: %s", key);
        CONFIG_UNLOCK();
        const char* fallback = default_value ? default_value : ""; // Return default or empty string, but never NULL
        char* result = heap_caps_strdup(fallback, MALLOC_CAP_SPIRAM);
        if (!result) {
            ESP_LOGE(TAG, "Failed to allocate memory for fallback string.");
        }
        return result;
    }

    if (entry->valid) {
        ESP_LOGD(TAG, "Cache hit for string key: %s", key);
        char* result = heap_caps_strdup(entry->data.str_value, MALLOC_CAP_SPIRAM);
        if (!result) {
            ESP_LOGE(TAG, "Failed to allocate memory for cached string.");
        }
        CONFIG_UNLOCK();
        return result;
    }

    ESP_LOGI(TAG, "Cache miss for string key: %s. Reading from NVS.", key);

    nvs_handle handle;
    char* loaded_value = NULL;

    esp_err_t err = nvs_open(NVS_CONFIG_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_OK) {
        size_t size = 0;
        err = nvs_get_str(handle, key, NULL, &size);
        if (err == ESP_OK && size > 0) {
            loaded_value = (char*) heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
            if (loaded_value) {
                nvs_get_str(handle, key, loaded_value, &size);
            } else {
                ESP_LOGE(TAG, "Failed to allocate memory for NVS string key: %s", key);
            }
        } else if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "Key not found in NVS: %s. Using default value.", key);
        } else {
            ESP_LOGE(TAG, "Error reading key: %s from NVS. Error: %d", key, err);
        }
        nvs_close(handle);
    } else {
        ESP_LOGW(TAG, "Failed to open NVS for key: %s. Using default value.", key);
    }

    // If loading from NVS failed, use the default value.
    // If default_value is also NULL, use an empty string to prevent crashes.
    if (!loaded_value) {
        const char* source_str = default_value ? default_value : "";
        loaded_value = (char*) heap_caps_strdup(source_str, MALLOC_CAP_SPIRAM);
        // Final fallback if strdup fails
        if (!loaded_value) {
            ESP_LOGE(TAG, "Failed to allocate memory for fallback value.");
            CONFIG_UNLOCK();
            return NULL;
        }
    }

    if (err == ESP_OK || err == ESP_ERR_NVS_NOT_FOUND) {
        if (entry->data.str_value) {
            heap_caps_free(entry->data.str_value);
        }
        entry->data.str_value = loaded_value;
        entry->valid = true;
    } else {
        ESP_LOGW(TAG, "NVS read error for key %s. Cache entry remains invalid.", key);
        entry->valid = false;
    }

    char* result = heap_caps_strdup(loaded_value, MALLOC_CAP_SPIRAM);
    if (!result) {
        ESP_LOGE(TAG, "Failed to allocate memory for result string.");
    }

    CONFIG_UNLOCK();
    return result;
}

void nvs_config_set_string(const char *key, const char *value) {
    CONFIG_LOCK();
    ConfigCacheEntry* entry = get_cache_entry(key, CONFIG_TYPE_STRING);
    if (!entry) {
        ESP_LOGE(TAG, "Failed to get or create cache entry for key: %s", key);
        CONFIG_UNLOCK();
        return;
    }

    if (entry->type != CONFIG_TYPE_STRING) {
        ESP_LOGE(TAG, "Type mismatch for key: %s. Expected type CONFIG_TYPE_STRING, but found type %d.", key, entry->type);
        CONFIG_UNLOCK();
        return;
    }

    if (entry->valid && entry->data.str_value) {
        heap_caps_free(entry->data.str_value);
    }

    const char* source_str = value ? value : "";
    entry->data.str_value = (char*) heap_caps_strdup(source_str, MALLOC_CAP_SPIRAM);
    if (!entry->data.str_value) {
        ESP_LOGE(TAG, "Failed to allocate memory for string value for key: %s", key);
        entry->valid = false;
        entry->dirty = false;
        CONFIG_UNLOCK();
        return;
    }
    entry->valid = true;
    entry->dirty = true;
    CONFIG_UNLOCK();
}

// --- U16 Functions ---
uint16_t nvs_config_get_u16(const char *key, const uint16_t default_value) {
    CONFIG_LOCK();
    ConfigCacheEntry* entry = get_cache_entry(key, CONFIG_TYPE_U16);
    if (!entry) {
        ESP_LOGE(TAG, "Failed to get or create cache entry for key: %s", key);
        CONFIG_UNLOCK();
        return default_value;
    }

    if (entry->valid) {
        ESP_LOGD(TAG, "Cache hit for u16 key: %s", key);
        uint16_t val = entry->data.u16_value;
        CONFIG_UNLOCK();
        return val;
    }

    ESP_LOGI(TAG, "Cache miss for u16 key: %s. Reading from NVS.", key);
    nvs_handle handle;
    uint16_t val = default_value;
    esp_err_t err = nvs_open(NVS_CONFIG_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_OK) {
        err = nvs_get_u16(handle, key, &val);
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "Key not found in NVS: %s. Using default value.", key);
        } else if (err != ESP_OK) {
            ESP_LOGE(TAG, "Error reading key: %s from NVS. Error: %d", key, err);
        }
        nvs_close(handle);
    } else {
        ESP_LOGE(TAG, "Failed to open NVS for key: %s. Using default value.", key);
    }

    if (err == ESP_OK || err == ESP_ERR_NVS_NOT_FOUND) {
        entry->data.u16_value = val;
        entry->valid = true;
    } else {
        ESP_LOGW(TAG, "NVS read error for key %s. Cache entry remains invalid.", key);
        entry->valid = false;
    }

    CONFIG_UNLOCK();
    return val;
}

void nvs_config_set_u16(const char *key, const uint16_t value) {
    CONFIG_LOCK();
    ConfigCacheEntry* entry = get_cache_entry(key, CONFIG_TYPE_U16);
    if (!entry) {
        ESP_LOGE(TAG, "Failed to get or create cache entry for key: %s", key);
        CONFIG_UNLOCK();
        return;
    }

    if (entry->type != CONFIG_TYPE_U16) {
        ESP_LOGE(TAG, "Type mismatch for key: %s. Expected type CONFIG_TYPE_U16, but found type %d.", key, entry->type);
        CONFIG_UNLOCK();
        return;
    }

    entry->data.u16_value = value;
    entry->valid = true;
    entry->dirty = true;
    CONFIG_UNLOCK();
}

// --- U64 Functions ---
uint64_t nvs_config_get_u64(const char *key, const uint64_t default_value) {
    CONFIG_LOCK();
    ConfigCacheEntry* entry = get_cache_entry(key, CONFIG_TYPE_U64);
    if (!entry) {
        ESP_LOGE(TAG, "Failed to get or create cache entry for key: %s", key);
        CONFIG_UNLOCK();
        return default_value;
    }

    if (entry->valid) {
        ESP_LOGD(TAG, "Cache hit for u64 key: %s", key);
        uint64_t val = entry->data.u64_value;
        CONFIG_UNLOCK();
        return val;
    }

    ESP_LOGI(TAG, "Cache miss for u64 key: %s. Reading from NVS.", key);
    nvs_handle handle;
    uint64_t val = default_value;
    esp_err_t err = nvs_open(NVS_CONFIG_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_OK) {
        err = nvs_get_u64(handle, key, &val);
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "Key not found in NVS: %s. Using default value.", key);
        } else if (err != ESP_OK) {
            ESP_LOGE(TAG, "Error reading key: %s from NVS. Error: %d", key, err);
        }
        nvs_close(handle);
    } else {
        ESP_LOGE(TAG, "Failed to open NVS for key: %s. Using default value.", key);
    }

    if (err == ESP_OK || err == ESP_ERR_NVS_NOT_FOUND) {
        entry->data.u64_value = val;
        entry->valid = true;
    } else {
        ESP_LOGW(TAG, "NVS read error for key %s. Cache entry remains invalid.", key);
        entry->valid = false;
    }

    CONFIG_UNLOCK();
    return val;
}

void nvs_config_set_u64(const char *key, const uint64_t value) {
    CONFIG_LOCK();
    ConfigCacheEntry* entry = get_cache_entry(key, CONFIG_TYPE_U64);
    if (!entry) {
        ESP_LOGE(TAG, "Failed to get or create cache entry for key: %s", key);
        CONFIG_UNLOCK();
        return;
    }

    if (entry->type != CONFIG_TYPE_U64) {
        ESP_LOGE(TAG, "Type mismatch for key: %s. Expected type CONFIG_TYPE_U64, but found type %d.", key, entry->type);
        CONFIG_UNLOCK();
        return;
    }

    entry->data.u64_value = value;
    entry->valid = true;
    entry->dirty = true;
    CONFIG_UNLOCK();
}

// --- flush changes to NVS ---
void flush_nvs_changes() {
    ConfigCacheEntry dirty_copy[MAX_CONFIG_ENTRIES] = {0};
    int dirty_count = 0;

    ESP_LOGI(TAG, "Scanning for dirty entries to flush.");
    CONFIG_LOCK();
    for (int i = 0; i < MAX_CONFIG_ENTRIES && dirty_count < MAX_CONFIG_ENTRIES; ++i) {
        if (config_cache[i].key && config_cache[i].dirty) {
            memcpy(&dirty_copy[dirty_count], &config_cache[i], sizeof(ConfigCacheEntry));
            dirty_copy[dirty_count].key = config_cache[i].key;

            if (dirty_copy[dirty_count].type == CONFIG_TYPE_STRING && config_cache[i].data.str_value) {
                dirty_copy[dirty_count].data.str_value = strdup(config_cache[i].data.str_value);
                if (!dirty_copy[dirty_count].data.str_value) {
                    ESP_LOGE(TAG, "Failed to duplicate string for key: %s. Skipping entry.", config_cache[i].key);
                    continue; // skips entry if mem allocation fails
                }
            }
            dirty_count++;
        }
    }
    CONFIG_UNLOCK();

    if (dirty_count == 0) {
        ESP_LOGI(TAG, "No dirty entries to flush.");
        return;
    }

    ESP_LOGI(TAG, "Found %d dirty entries. Flushing to NVS...", dirty_count);
    nvs_handle handle;
    esp_err_t err = nvs_open(NVS_CONFIG_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not open NVS for flushing");
        for(int i = 0; i < dirty_count; ++i) {
            if(dirty_copy[i].type == CONFIG_TYPE_STRING && dirty_copy[i].data.str_value) {
                free(dirty_copy[i].data.str_value);
            }
        }
        return;
    }

    for (int i = 0; i < dirty_count; ++i) {
        switch (dirty_copy[i].type) {
            case CONFIG_TYPE_STRING:
                if (dirty_copy[i].data.str_value) {
                    nvs_set_str(handle, dirty_copy[i].key, dirty_copy[i].data.str_value);
                    free(dirty_copy[i].data.str_value);
                }
                break;
            case CONFIG_TYPE_U16:
                nvs_set_u16(handle, dirty_copy[i].key, dirty_copy[i].data.u16_value);
                break;
            case CONFIG_TYPE_U64:
                nvs_set_u64(handle, dirty_copy[i].key, dirty_copy[i].data.u64_value);
                break;
        }
    }

    nvs_commit(handle);
    nvs_close(handle);

    CONFIG_LOCK();
    for (int i = 0; i < dirty_count; ++i) {
        for (int j = 0; j < MAX_CONFIG_ENTRIES; ++j) {
            // Use strcmp for guaranteed correctness
            if (config_cache[j].key && strcmp(config_cache[j].key, dirty_copy[i].key) == 0) {
                config_cache[j].dirty = false;
                break;
            }
        }
    }
    CONFIG_UNLOCK();

    ESP_LOGI(TAG, "Configuration changes flushed to NVS.");
}

// --- caching check utility ---
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

// --- Debugging utility ---
void dump_cache_status() {
    ESP_LOGI(TAG, "--- Dumping Config Cache Status ---");
    bool is_cache_empty = true;

    CONFIG_LOCK();
    for (int i = 0; i < MAX_CONFIG_ENTRIES; ++i) {
        if (config_cache[i].key) {
            is_cache_empty = false;
            ESP_LOGI(TAG, "[%2d] key=%-18s, valid=%d, dirty=%d, type=%d",
                     i,
                     config_cache[i].key,
                     config_cache[i].valid,
                     config_cache[i].dirty,
                     config_cache[i].type);
        }
    }
    CONFIG_UNLOCK();

    if (is_cache_empty) {
        ESP_LOGI(TAG, "Config Cache is empty.");
    }

    ESP_LOGI(TAG, "--- End of Cache Dump ---");
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
