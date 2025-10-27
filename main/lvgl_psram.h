#pragma once

#include <string.h>

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "macros.h"

// Custom malloc function using PSRAM
static void *lv_psram_alloc(size_t size) {
    return MALLOC(size);
}

// Custom free function
static void lv_psram_free(void *ptr) {
    heap_caps_free(ptr);
}

static void *lv_psram_realloc(void *ptr, size_t new_size) {
    if (new_size == 0) {
        heap_caps_free(ptr);
        return NULL;
    }

    if (!ptr) {
        return MALLOC(new_size);
    }

    // Try to expand in place (not possible with ESP-IDF heap_caps)
    void *new_ptr = MALLOC(new_size);
    if (new_ptr) {
        // Copy old data to new location
        memcpy(new_ptr, ptr, new_size);  // Note: Might copy too much if shrinking
        heap_caps_free(ptr);
    }

    return new_ptr;
}
