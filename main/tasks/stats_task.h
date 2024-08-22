#include "esp_psram.h"
#pragma once

// 128k samples should be enough^^
// must be power of two
#define MAX_SAMPLES 0x20000
#define WRAP(a) ((a) & (MAX_SAMPLES - 1))

typedef struct {
    int first_sample;
    int last_sample;
    uint64_t timespan;
    uint64_t diffsum;
    double avg;
    double avg_gh;
} avg_t;

typedef struct {
    int num_samples;
    uint32_t shares[MAX_SAMPLES]; // pool diff is always 32bit int
    uint64_t timestamps[MAX_SAMPLES];
    float hashrate_10m[MAX_SAMPLES];
    float hashrate_1h[MAX_SAMPLES];
    float hashrate_1d[MAX_SAMPLES];
} psram_t;

typedef struct {
    float *hashrate_10m;
    float *hashrate_1h;
    float *hashrate_1d;
    uint64_t *timestamps;
} history_t;


void *stats_task(void *pvParameters);
double stats_task_get_10m();
double stats_task_get_1h();
double stats_task_get_1d();
void stats_task_push_share(uint32_t diff, uint64_t timestamp);
int history_search_nearest_timestamp(uint64_t timestamp);
void history_lock_psram();
void history_unlock_psram();
psram_t *history_get_psram();

inline uint64_t history_get_timestamp_sample(int index);
inline float history_get_hashrate_10m_sample(int index);
inline float history_get_hashrate_1h_sample(int index);
inline float history_get_hashrate_1d_sample(int index);
