#include "esp_log.h"
#include "esp_timer.h" // Include esp_timer for esp_timer_get_time
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "global_state.h"
#include <math.h>
#include <pthread.h>
#include <stdint.h>

#include "stats_task.h"

static const char *TAG = "stats_task";

static pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t stats_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t psram_mutex = PTHREAD_MUTEX_INITIALIZER;

static const int interval = 5;

// timespans in us, same resolution as the esp timer
static avg_t avg_10m = {
    .first_sample = 0, .last_sample = 0, .timespan = 600llu * 1000000llu, .diffsum = 0, .avg = 0.0, .avg_gh = 0.0};
static avg_t avg_1h = {
    .first_sample = 0, .last_sample = 0, .timespan = 3600llu * 1000000llu, .diffsum = 0, .avg = 0.0, .avg_gh = 0.0};
static avg_t avg_1d = {
    .first_sample = 0, .last_sample = 0, .timespan = 86400llu * 1000000llu, .diffsum = 0, .avg = 0.0, .avg_gh = 0.0};

// const static double alpha = 0.8;

static double smoothed_10m = 0.0;
static double smoothed_1h = 0.0;
static double smoothed_1d = 0.0;

// round(log(MAX_SAMPLES)/log(2))
const int MAX_SAMPLES_BITS = (int) (log2f(MAX_SAMPLES) + 0.5f);

psram_t *psram = 0;

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

static void stats_timer_ticker(TimerHandle_t xTimer)
{
    pthread_mutex_lock(&stats_mutex);
    pthread_cond_signal(&stats_cond);
    pthread_mutex_unlock(&stats_mutex);
}

// move avg window and track and adjust the total sum of all shares in the
// desired time window. Calculates GH.
void update_avg(avg_t *avg)
{
    // Catch up with the latest sample and update diffsum
    uint64_t last_timestamp = 0;
    while (last_timestamp = psram->timestamps[WRAP(avg->last_sample)], avg->last_sample + 1 < psram->num_samples) {
        avg->last_sample++;
        avg->diffsum += (uint64_t) psram->shares[WRAP(avg->last_sample)];
    }

    // Adjust the window on the older side
    uint64_t first_timestamp = 0;
    while (first_timestamp = psram->timestamps[WRAP(avg->first_sample)], (last_timestamp - first_timestamp) > avg->timespan) {
        avg->diffsum -= (uint64_t) psram->shares[WRAP(avg->first_sample)];
        avg->first_sample++;
    }

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

    avg->avg = (double) (avg->diffsum << 32llu) / ((double) duration / 1.0e6);
    avg->avg_gh = avg->avg / 1.0e9;
}

void history_lock_psram()
{
    pthread_mutex_lock(&psram_mutex);
}

void history_unlock_psram()
{
    pthread_mutex_unlock(&psram_mutex);
}

void stats_task_push_share(uint32_t diff, uint64_t timestamp)
{
    if (!psram) {
        ESP_LOGW(TAG, "PSRAM not initialized");
        return;
    }

    // use stratum client time
    struct timeval now;
    gettimeofday(&now, NULL);
    timestamp = (uint64_t) now.tv_sec * 1000000llu;

    history_lock_psram();
    psram->shares[WRAP(psram->num_samples)] = diff;
    psram->timestamps[WRAP(psram->num_samples)] = timestamp;
    psram->num_samples++;

    update_avg(&avg_10m);
    update_avg(&avg_1h);
    update_avg(&avg_1d);

    psram->hashrate_10m[WRAP(psram->num_samples - 1)] = avg_10m.avg_gh;
    psram->hashrate_1h[WRAP(psram->num_samples - 1)] = avg_1h.avg_gh;
    psram->hashrate_1d[WRAP(psram->num_samples - 1)] = avg_1d.avg_gh;
    history_unlock_psram();

    ESP_LOGI(TAG, "%llu hashrate: 10m:%.3fGH 1h:%.3fGH 1d:%.3fGH", timestamp, avg_10m.avg_gh, avg_1h.avg_gh, avg_1d.avg_gh);
}

double stats_task_get_10m()
{
    return avg_10m.avg_gh;
}

double stats_task_get_1h()
{
    return avg_1h.avg_gh;
}

double stats_task_get_1d()
{
    return avg_1d.avg_gh;
}

psram_t *history_get_psram()
{
    return psram;
}

// successive approximation in a wrapped ring buffer with
// monotonic/unwrapped write pointer :woozy:
int history_search_nearest_timestamp(uint64_t timestamp)
{
    // get index of the first sample, clamp to min 0
    int lowest_index = (psram->num_samples - MAX_SAMPLES < 0) ? 0 : psram->num_samples - MAX_SAMPLES;

    // last sample
    int highest_index = psram->num_samples - 1;

    ESP_LOGI(TAG, "lowest_index: %d highest_index: %d", lowest_index, highest_index);

    // remove before flight
    if (psram->timestamps[WRAP(lowest_index)] > psram->timestamps[WRAP(highest_index)]) {
        ESP_LOGE(TAG, "something is wrong");
        return -1;
    }

    int current = 0;
    int num_elements = 0;

    while (current = (highest_index + lowest_index) / 2, num_elements = highest_index - lowest_index + 1, num_elements > 1) {
        // Get timestamp at the current index, wrapping as necessary
        uint64_t stored_timestamp = psram->timestamps[WRAP(current)];
        ESP_LOGI(TAG, "current %d num_elements %d stored_timestamp %llu wrapped-current %d", current, num_elements, stored_timestamp,
                 WRAP(current));

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

    ESP_LOGI(TAG, "current return %d", current);

    return current;
}

static void forever()
{
    while (1) {
        vTaskDelay(100000 / portTICK_PERIOD_MS);
    }
}

void *stats_task(void *pvParameters)
{
    GlobalState *GLOBAL_STATE = (GlobalState *) pvParameters;

    SystemModule *module = &GLOBAL_STATE->SYSTEM_MODULE;

    psram = (psram_t *) heap_caps_malloc(sizeof(psram_t), MALLOC_CAP_SPIRAM);
    if (!psram) {
        ESP_LOGE(TAG, "Couldn't allocate memory of PSRAM");
        forever();
    }

    psram->num_samples = 0;

    ESP_LOGI(TAG, "Stats Interval: %dms", interval * 1000);

    // Create a timer
    TimerHandle_t stats_timer = xTimerCreate(TAG, pdMS_TO_TICKS(interval * 1000), pdTRUE, NULL, stats_timer_ticker);

    if (stats_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create timer");
        return NULL;
    }

    // Start the timer
    if (xTimerStart(stats_timer, 0) != pdPASS) {
        ESP_LOGE(TAG, "Failed to start timer");
        return NULL;
    }

    while (1) {
        pthread_mutex_lock(&stats_mutex);
        pthread_cond_wait(&stats_cond, &stats_mutex); // Wait for the timer or external trigger
        pthread_mutex_unlock(&stats_mutex);
        /*
                // do some resampling to look nicer
                smoothed_10m = (1.0 - alpha) * smoothed_10m + avg_10m.avg_gh * alpha;
                smoothed_1h = (1.0 - alpha) * smoothed_1h + avg_1h.avg_gh * alpha;
                smoothed_1d = (1.0 - alpha) * smoothed_1d + avg_1d.avg_gh * alpha;
        */
    }
}
