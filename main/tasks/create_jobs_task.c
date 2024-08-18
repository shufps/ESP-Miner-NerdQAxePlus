#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "global_state.h"
#include "mining.h"
#include "string.h"
#include "system.h"
#include <limits.h>

#include <sys/time.h>

#ifdef DEBUG_MEMORY_LOGGING
#include "leak_tracker.h"
#endif

static const char * TAG = "create_jobs_task";

static uint32_t last_pool_diff = 0;
static uint32_t last_ntime = 0;
static uint64_t last_submit_time = 0;
static uint32_t extranonce_2 = 0;

GlobalState * GLOBAL;

static void create_job_timer(TimerHandle_t xTimer)
{
    pthread_mutex_lock(&GLOBAL->current_stratum_job_lock);

    // be lazy and reduce so much typing
    mining_notify * current_job = &GLOBAL->current_stratum_job;

    // do we have some job yet?
    if (!current_job->ntime) {
        pthread_mutex_unlock(&GLOBAL->current_stratum_job_lock);
        return;
    }
    // check the ntime, new jobs have different ntime
    // is faster than checking the job id
    if (last_ntime != current_job->ntime) {
        last_ntime = current_job->ntime;
        ESP_LOGI(TAG, "New Work Received %s", current_job->job_id);
    }

    char * extranonce_2_str = extranonce_2_generate(extranonce_2, GLOBAL->extranonce_2_len);

    char * coinbase_tx =
        construct_coinbase_tx(current_job->coinbase_1, current_job->coinbase_2, GLOBAL->extranonce_str, extranonce_2_str);

    char * merkle_root =
        calculate_merkle_root_hash(coinbase_tx, (uint8_t(*)[32]) current_job->_merkle_branches, current_job->n_merkle_branches);

    bm_job * next_job = construct_bm_job(current_job, merkle_root, GLOBAL->version_mask);

    next_job->jobid = strdup(current_job->job_id);
    next_job->extranonce2 = strdup(extranonce_2_str);
    next_job->pool_diff = GLOBAL->stratum_difficulty;

    pthread_mutex_unlock(&GLOBAL->current_stratum_job_lock);

    if (next_job->pool_diff != last_pool_diff) {
        ESP_LOGI(TAG, "New pool difficulty %lu", next_job->pool_diff);
        last_pool_diff = next_job->pool_diff;

        // adjust difficulty on asic
        (*GLOBAL->ASIC_functions.set_difficulty_mask_fn)(next_job->pool_diff);
    }
    uint64_t current_time = esp_timer_get_time();
    if (last_submit_time) {
        ESP_LOGI(TAG, "job interval %dms", (int) ((current_time - last_submit_time) / 1e3));
    }
    last_submit_time = current_time;

    (*GLOBAL->ASIC_functions.send_work_fn)(GLOBAL, next_job); // send the job to the ASIC

    free(coinbase_tx);
    free(merkle_root);
    free(extranonce_2_str);
    extranonce_2++;
}

void create_jobs_task(void * pvParameters)
{
    GLOBAL = (GlobalState *) pvParameters;

    ESP_LOGI(TAG, "ASIC Job Interval: %.2f ms", GLOBAL->asic_job_frequency_ms);
    SYSTEM_notify_mining_started(&GLOBAL);
    ESP_LOGI(TAG, "ASIC Ready!");

    // Create the timer
    TimerHandle_t job_timer = xTimerCreate(TAG,                                                // Timer name
                                           pdMS_TO_TICKS(GLOBAL->asic_job_frequency_ms), // Timer period
                                           pdTRUE,                                             // Auto-reload (yes)
                                           NULL,                                               // Pass parameter
                                           create_job_timer                                    // Callback function
    );

    if (job_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create timer");
    } else {
        // Start the timer
        if (xTimerStart(job_timer, 0) != pdPASS) {
            ESP_LOGE(TAG, "Failed to start timer");
        }
    }

    // sleep forever
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
