#include "esp_log.h"
#include "esp_timer.h" // Include esp_timer for esp_timer_get_time
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "global_state.h"
#include <math.h>
#include <pthread.h>
#include <stdint.h>

#include "history.h"

#pragma GCC diagnostic error "-Wall"
#pragma GCC diagnostic error "-Wextra"
#pragma GCC diagnostic error "-Wmissing-prototypes"

static const char *TAG = "history";

static pthread_mutex_t psram_mutex = PTHREAD_MUTEX_INITIALIZER;

static int num_asics = 0;
static uint32_t *distribution = NULL;

// timespans in ms
static avg_t avg_10m = {.first_sample = 0,
                        .last_sample = 0,
                        .timespan = 600llu * 1000llu,
                        .diffsum = 0,
                        .avg = 0.0,
                        .avg_gh = 0.0,
                        .timestamp = 0,
                        .preliminary = true};
static avg_t avg_1h = {.first_sample = 0,
                       .last_sample = 0,
                       .timespan = 3600llu * 1000llu,
                       .diffsum = 0,
                       .avg = 0.0,
                       .avg_gh = 0.0,
                       .timestamp = 0,
                       .preliminary = true};
static avg_t avg_1d = {.first_sample = 0,
                       .last_sample = 0,
                       .timespan = 86400llu * 1000llu,
                       .diffsum = 0,
                       .avg = 0.0,
                       .avg_gh = 0.0,
                       .timestamp = 0,
                       .preliminary = true};

psram_t *psram = 0;

// define for wrapped access of psram
#define WRAP(a) ((a) & (HISTORY_MAX_SAMPLES - 1))

inline uint64_t history_get_timestamp_sample(int index)
{
    return psram->timestamps[WRAP(index)];
}

inline float history_get_hashrate_10m_sample(int index)
{
    return psram->hashrate_10m[WRAP(index)];
}

inline float history_get_hashrate_1h_sample(int index)
{
    return psram->hashrate_1h[WRAP(index)];
}

inline float history_get_hashrate_1d_sample(int index)
{
    return psram->hashrate_1d[WRAP(index)];
}

inline uint32_t history_get_share_sample(int index)
{
    return psram->shares[WRAP(index)];
}

double history_get_current_10m()
{
    return avg_10m.avg_gh;
}

double history_get_current_1h()
{
    return avg_1h.avg_gh;
}

double history_get_current_1d()
{
    return avg_1d.avg_gh;
}

uint64_t history_get_current_timestamp()
{
    // all timestamps are equal
    return avg_10m.timestamp;
}

void history_lock()
{
    pthread_mutex_lock(&psram_mutex);
}

void history_unlock()
{
    pthread_mutex_unlock(&psram_mutex);
}

bool is_history_available()
{
    return psram != 0;
}

void history_get_timestamps(uint64_t *first, uint64_t *last, int *num_samples)
{
    int lowest_index = (psram->num_samples - HISTORY_MAX_SAMPLES < 0) ? 0 : psram->num_samples - HISTORY_MAX_SAMPLES;
    int highest_index = psram->num_samples - 1;

    int _num_samples = highest_index - lowest_index + 1;

    uint64_t first_timestamp = (_num_samples) ? history_get_timestamp_sample(lowest_index) : 0;
    uint64_t last_timestamp = (_num_samples) ? history_get_timestamp_sample(highest_index) : 0;

    *first = first_timestamp;
    *last = last_timestamp;
    *num_samples = _num_samples;
}

// move avg window and track and adjust the total sum of all shares in the
// desired time window. Calculates GH.
// calculates incrementally without "scanning" the entire time span
static void update_avg(avg_t *avg)
{
    // Catch up with the latest sample and update diffsum
    uint64_t last_timestamp = 0;
    while (last_timestamp = history_get_timestamp_sample(avg->last_sample), avg->last_sample + 1 < psram->num_samples) {
        avg->last_sample++;
        avg->diffsum += (uint64_t) history_get_share_sample(avg->last_sample);
    }

    // adjust the window on the older side
    // we move the lower window bound until the next sample would be out of
    // the desired timespan.
    while (avg->first_sample + 1 < avg->last_sample &&
           last_timestamp - history_get_timestamp_sample(avg->first_sample + 1) >= avg->timespan) {
        avg->diffsum -= (uint64_t) history_get_share_sample(avg->first_sample);
        avg->first_sample++;
    }

    uint64_t first_timestamp = history_get_timestamp_sample(avg->first_sample);

    // Check for overflow in diffsum
    if (avg->diffsum >> 63ull) {
        ESP_LOGE(TAG, "Error in hashrate calculation: diffsum overflowed");
        return;
    }

    // Prevent division by zero
    if (last_timestamp == first_timestamp) {
        ESP_LOGW(TAG, "Timestamps are equal; cannot compute average.");
        return;
    }

    // Calculate the average hash rate
    uint64_t duration = (last_timestamp - first_timestamp);

    // clamp duration to a minimum value of avg->timespan
    duration = (avg->timespan > duration) ? avg->timespan : duration;

    avg->avg = (double) (avg->diffsum << 32llu) / ((double) duration / 1.0e3);
    avg->avg_gh = avg->avg / 1.0e9;
    avg->timestamp = last_timestamp;
    avg->preliminary = duration >= avg->timespan;
}

static void log_distribution()
{
    // this can happen if we don't have asics
    if (!distribution) {
        return;
    }

    char buffer[256];
    size_t offset = 0;

    // Iterate through each ASIC and append its count to the log message
    for (int i = 0; i < num_asics; i++) {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "%lu/", distribution[i]);
    }
    if (offset > 0) {
        buffer[offset - 1] = 0; // remove trailing slash
    }

    ESP_LOGI(TAG, "nonce distribution: %s", buffer);
}

void history_push_share(uint32_t diff, uint64_t timestamp, int asic_nr)
{
    if (!psram) {
        ESP_LOGW(TAG, "PSRAM not initialized");
        return;
    }

    history_lock();
    psram->shares[WRAP(psram->num_samples)] = diff;
    psram->timestamps[WRAP(psram->num_samples)] = timestamp;
    psram->num_samples++;

    update_avg(&avg_10m);
    update_avg(&avg_1h);
    update_avg(&avg_1d);

    psram->hashrate_10m[WRAP(psram->num_samples - 1)] = avg_10m.avg_gh;
    psram->hashrate_1h[WRAP(psram->num_samples - 1)] = avg_1h.avg_gh;
    psram->hashrate_1d[WRAP(psram->num_samples - 1)] = avg_1d.avg_gh;

    if (distribution && asic_nr < num_asics) {
        distribution[asic_nr]++;
    }

    history_unlock();

    char preliminary_10m = (avg_10m.preliminary) ? '*' : ' ';
    char preliminary_1h = (avg_1h.preliminary) ? '*' : ' ';
    char preliminary_1d = (avg_1d.preliminary) ? '*' : ' ';

    ESP_LOGI(TAG, "%llu hashrate: 10m:%.3fGH%c 1h:%.3fGH%c 1d:%.3fGH%c", timestamp, avg_10m.avg_gh, preliminary_10m, avg_1h.avg_gh,
             preliminary_1h, avg_1d.avg_gh, preliminary_1d);

    log_distribution();
}

// successive approximation in a wrapped ring buffer with
// monotonic/unwrapped write pointer :woozy:
int history_search_nearest_timestamp(uint64_t timestamp)
{
    // get index of the first sample, clamp to min 0
    int lowest_index = (psram->num_samples - HISTORY_MAX_SAMPLES < 0) ? 0 : psram->num_samples - HISTORY_MAX_SAMPLES;

    // last sample
    int highest_index = psram->num_samples - 1;

    ESP_LOGD(TAG, "lowest_index: %d highest_index: %d", lowest_index, highest_index);

    int current = 0;
    int num_elements = 0;

    while (current = (highest_index + lowest_index) / 2, num_elements = highest_index - lowest_index + 1, num_elements > 1) {
        // Get timestamp at the current index, wrapping as necessary
        uint64_t stored_timestamp = history_get_timestamp_sample(current);
        ESP_LOGD(TAG, "current %d num_elements %d stored_timestamp %llu wrapped-current %d", current, num_elements,
                 stored_timestamp, WRAP(current));

        if (stored_timestamp > timestamp) {
            // If timestamp is too large, search lower
            highest_index = current - 1; // Narrow the search to the lower half
        } else if (stored_timestamp < timestamp) {
            // If timestamp is too small, search higher
            lowest_index = current + 1; // Narrow the search to the upper half
        } else {
            // Exact match found
            return current;
        }
    }

    ESP_LOGD(TAG, "current return %d", current);

    if (current < 0 || current >= psram->num_samples) {
        return -1;
    }

    return current;
}

bool history_init(int _num_asics)
{
    psram = (psram_t *) heap_caps_malloc(sizeof(psram_t), MALLOC_CAP_SPIRAM);
    if (!psram) {
        ESP_LOGE(TAG, "Couldn't allocate memory of PSRAM");
        return false;
    }
    psram->num_samples = 0;

    num_asics = _num_asics;
    if (num_asics > 0) {
        // calloc zero-initializes
        distribution = (uint32_t *) calloc(num_asics, sizeof(uint32_t));
    }

    return true;
}
