#pragma once

#include "ArduinoJson.h"
#include "esp_heap_caps.h"

struct PSRAMAllocator : ArduinoJson::Allocator {
    void* allocate(size_t size) override {
        return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    }

    void deallocate(void* pointer) override {
        heap_caps_free(pointer);
    }

    void* reallocate(void* ptr, size_t new_size) override {
        return heap_caps_realloc(ptr, new_size, MALLOC_CAP_SPIRAM);
    }
};

