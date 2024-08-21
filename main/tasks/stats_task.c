#include "esp_log.h"
#include "esp_timer.h" // Include esp_timer for esp_timer_get_time
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "global_state.h"
#include <pthread.h>
#include <stdint.h>

#include "stats_task.h"

static const char *TAG = "stats_task";

static pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t stats_cond = PTHREAD_COND_INITIALIZER;

static const int interval = 5;

// timespans in us, same resolution as the esp timer
static avg_t avg_10m = {.first_sample = 0, .last_sample = 0, .timespan = 600llu * 1000000llu, .diffsum = 0, .avg = 0.0, .avg_gh = 0.0};
static avg_t avg_1h = {.first_sample = 0, .last_sample = 0, .timespan = 3600llu * 1000000llu, .diffsum = 0, .avg = 0.0, .avg_gh = 0.0};
static avg_t avg_1d = {.first_sample = 0, .last_sample = 0, .timespan = 86400llu * 1000000llu, .diffsum = 0, .avg = 0.0, .avg_gh = 0.0};


//const static double alpha = 0.8;

static double smoothed_10m = 0.0;
static double smoothed_1h = 0.0;
static double smoothed_1d = 0.0;

psram_t *psram = 0;

#define WRAP(a) ((a) & (MAX_SAMPLES - 1))

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

void stats_task_push_share(uint32_t diff, uint64_t timestamp)
{
    if (!psram) {
        ESP_LOGW(TAG, "PSRAM not initialized");
        return;
    }

    psram->shares[WRAP(psram->num_samples)] = diff;
    psram->timestamps[WRAP(psram->num_samples)] = timestamp;
    psram->num_samples++;

    update_avg(&avg_10m);
    update_avg(&avg_1h);
    update_avg(&avg_1d);

    ESP_LOGI(TAG, "hashrate: 10m:%.3fGH 1h:%.3fGH 1d:%.3fGH", avg_10m.avg_gh, avg_1h.avg_gh, avg_1d.avg_gh);
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

    if (!esp_psram_is_initialized()) {
        ESP_LOGE(TAG, "PSRAM is not available");
        forever();
    }

    size_t total_psram = esp_psram_get_size();
    ESP_LOGI(TAG, "PSRAM found with %dMB", total_psram / (1024 * 1024));

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
