#pragma once

#include <string.h>

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "macros.h"

static inline void* lv_psram_alloc(size_t size) {
    return size ? MALLOC(size) : NULL;
}

static inline void lv_psram_free(void* ptr) {
    FREE(ptr);
}

static inline void* lv_psram_realloc(void* ptr, size_t new_size) {
    return REALLOC(ptr, new_size);
}