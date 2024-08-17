#include "global_state.h"
#include "esp_log.h"
#include "esp_system.h"
#include "mining.h"
#include <limits.h>
#include "string.h"
#include "system.h"

#include <sys/time.h>

#ifdef DEBUG_MEMORY_LOGGING
#include "leak_tracker.h"
#endif

static const char *TAG = "create_jobs_task";

void create_jobs_task(void *pvParameters)
{
    GlobalState *GLOBAL_STATE = (GlobalState *)pvParameters;

    uint32_t extranonce_2 = 0;

    ESP_LOGI(TAG, "ASIC Job Interval: %.2f ms", GLOBAL_STATE->asic_job_frequency_ms);
    SYSTEM_notify_mining_started(GLOBAL_STATE);
    ESP_LOGI(TAG, "ASIC Ready!");

    uint32_t last_ntime = 0;

    // be lazy and reduce so much typing
    mining_notify *current_job = &GLOBAL_STATE->current_stratum_job;

    uint32_t last_pool_diff = 0;

    while (1)
    {
        vTaskDelay((GLOBAL_STATE->asic_job_frequency_ms - 0.3) / portTICK_PERIOD_MS);

        pthread_mutex_lock(&GLOBAL_STATE->current_stratum_job_lock);

        // do we have some job yet?
        if (!current_job->ntime) {
            pthread_mutex_unlock(&GLOBAL_STATE->current_stratum_job_lock);
            continue;
        }
        // check the ntime, new jobs have different ntime
        // is faster than checking the job id
        if (last_ntime  != current_job->ntime) {
            last_ntime = current_job->ntime;
            ESP_LOGI(TAG, "New Work Received %s", current_job->job_id);
        }

        char *extranonce_2_str = extranonce_2_generate(extranonce_2, GLOBAL_STATE->extranonce_2_len);

        char *coinbase_tx = construct_coinbase_tx(current_job->coinbase_1, current_job->coinbase_2, GLOBAL_STATE->extranonce_str, extranonce_2_str);

        char *merkle_root = calculate_merkle_root_hash(coinbase_tx, (uint8_t(*)[32])current_job->_merkle_branches, current_job->n_merkle_branches);
        bm_job *next_job = construct_bm_job(current_job, merkle_root, GLOBAL_STATE->version_mask);

        next_job->jobid = strdup(current_job->job_id);
        next_job->extranonce2 = strdup(extranonce_2_str);
        next_job->pool_diff = GLOBAL_STATE->stratum_difficulty;

        pthread_mutex_unlock(&GLOBAL_STATE->current_stratum_job_lock);

        if (next_job->pool_diff != last_pool_diff)
        {
            ESP_LOGI(TAG, "New pool difficulty %lu", next_job->pool_diff);
            last_pool_diff = next_job->pool_diff;

            // adjust difficulty on asic
            (*GLOBAL_STATE->ASIC_functions.set_difficulty_mask_fn)(next_job->pool_diff);
        }

        (*GLOBAL_STATE->ASIC_functions.send_work_fn)(GLOBAL_STATE, next_job); // send the job to the ASIC

        free(coinbase_tx);
        free(merkle_root);
        free(extranonce_2_str);
        extranonce_2++;


    }
}
