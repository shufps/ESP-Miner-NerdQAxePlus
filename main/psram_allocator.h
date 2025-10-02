#pragma once

#include "ArduinoJson.h"
#include "esp_heap_caps.h"
#include "macros.h"

struct PSRAMAllocator : ArduinoJson::Allocator {
    void* allocate(size_t size) override {
        return MALLOC(size);
    }

    void deallocate(void* pointer) override {
        FREE(pointer);
    }

    void* reallocate(void* ptr, size_t new_size) override {
        return REALLOC(ptr, new_size);
    }
};

