#include "esp_psram.h"
#pragma once

// should be enough^^
// must be power of two
#define MAX_SAMPLES 0x10000

typedef struct {
    uint32_t first_sample;
    uint32_t last_sample;
    uint64_t timespan;
    uint64_t diffsum;
    double avg;
    double avg_gh;
} avg_t;

typedef struct {
    uint32_t num_samples;
    uint32_t shares[MAX_SAMPLES]; // pool diff is always 32bit int
    uint64_t timestamps[MAX_SAMPLES];
} psram_t;

void *stats_task(void *pvParameters);

void stats_task_push_share(uint32_t diff, uint64_t timestamp);