#include <stdlib.h>
#include <string.h>
#include "esp_log.h"

static const char *TAG = "MEMORY_LOG";

void *malloc_wrapper(size_t size, const char *file, int line) {
    void *ptr = malloc(size);
    if (ptr) {
        ESP_LOGI(TAG, "malloc: %zu bytes allocated at %p [File: %s, Line: %d, Addr: %08lx]", size, ptr, file, line, (uint32_t) ptr);
    } else {
        ESP_LOGE(TAG, "malloc failed [File: %s, Line: %d]", file, line);
    }
    return ptr;
}

void free_wrapper(void *ptr, const char *file, int line) {
    if (ptr) {
        ESP_LOGI(TAG, "free: memory freed at %p [File: %s, Line: %d, Addr: %08lx]", ptr, file, line, (uint32_t) ptr);
        free(ptr);
    } else {
        ESP_LOGW(TAG, "free: attempted to free a NULL pointer [File: %s, Line: %d]", file, line);
    }
}

// Wrapper for strdup
char *strdup_wrapper(const char *str, const char *file, int line) {
    char *ptr = strdup(str);
    if (ptr) {
        ESP_LOGI(TAG, "strdup: %zu bytes allocated at %p [File: %s, Line: %d, Addr: %08lx]", strlen(str) + 1, ptr, file, line, (uint32_t)ptr);
    } else {
        ESP_LOGE(TAG, "strdup failed [File: %s, Line: %d]", file, line);
    }
    return ptr;
}