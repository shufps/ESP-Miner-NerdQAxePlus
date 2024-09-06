#include "esp_psram.h"
#pragma once

// 128k samples should be enough^^
// must be power of two
#define HISTORY_MAX_SAMPLES 0x20000

typedef struct {
    int first_sample;
    int last_sample;
    uint64_t timespan;
    uint64_t diffsum;
    double avg;
    double avg_gh;
    uint64_t timestamp;
    bool preliminary;
} avg_t;

typedef struct {
    int num_samples;
    uint32_t shares[HISTORY_MAX_SAMPLES]; // pool diff is always 32bit int
    uint64_t timestamps[HISTORY_MAX_SAMPLES];   // in ms
    float hashrate_10m[HISTORY_MAX_SAMPLES];
    float hashrate_1h[HISTORY_MAX_SAMPLES];
    float hashrate_1d[HISTORY_MAX_SAMPLES];
} psram_t;

typedef struct {
    float *hashrate_10m;
    float *hashrate_1h;
    float *hashrate_1d;
    uint64_t *timestamps;   // in ms
} history_t;


void *stats_task(void *pvParameters);

bool history_init(int num_asics);
void history_push_share(uint32_t diff, uint64_t timestamp, int asic_nr);
int history_search_nearest_timestamp(uint64_t timestamp);

uint64_t history_get_timestamp_sample(int index);
float history_get_hashrate_10m_sample(int index);
float history_get_hashrate_1h_sample(int index);
float history_get_hashrate_1d_sample(int index);
double history_get_current_10m(void);
double history_get_current_1h(void);
double history_get_current_1d(void);
uint64_t history_get_current_timestamp(void);
void history_lock(void);
void history_unlock(void);
bool is_history_available(void);
void history_get_timestamps(uint64_t *first, uint64_t *last, int *num_samples);

