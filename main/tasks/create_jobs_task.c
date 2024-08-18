#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "global_state.h"
#include "mining.h"
#include "string.h"
#include "system.h"
#include <limits.h>

#include <pthread.h>
#include <sys/time.h>

#ifdef DEBUG_MEMORY_LOGGING
#include "leak_tracker.h"
#endif

static const char * TAG = "create_jobs_task";

pthread_mutex_t job_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t job_cond = PTHREAD_COND_INITIALIZER;

pthread_mutex_t current_stratum_job_mutex = PTHREAD_MUTEX_INITIALIZER;

static mining_notify current_job;

static char * extranonce_str = NULL;
static int extranonce_2_len = 0;

static uint32_t stratum_difficulty = 8192;
static uint32_t version_mask = 0;

static void create_job_timer(TimerHandle_t xTimer)
{
    pthread_mutex_lock(&job_mutex);
    pthread_cond_signal(&job_cond);
    pthread_mutex_unlock(&job_mutex);
}

void trigger_job_creation()
{
    pthread_mutex_lock(&job_mutex);
    pthread_cond_signal(&job_cond);
    pthread_mutex_unlock(&job_mutex);
}

void create_job_set_version_mask(uint32_t mask) {
    pthread_mutex_lock(&current_stratum_job_mutex);
    version_mask = mask;
    pthread_mutex_unlock(&current_stratum_job_mutex);
}

bool create_job_set_difficulty(uint32_t diffituly) {
    pthread_mutex_lock(&current_stratum_job_mutex);

    // new difficulty?
    bool is_new = stratum_difficulty != diffituly;

    // set difficulty
    stratum_difficulty = diffituly;
    pthread_mutex_unlock(&current_stratum_job_mutex);
    return is_new;
}

void create_job_set_enonce(char* enonce, int enonce2_len) {
    pthread_mutex_lock(&current_stratum_job_mutex);
    if (extranonce_str) {
        free(extranonce_str);
    }
    extranonce_str = strdup(enonce);
    extranonce_2_len = enonce2_len;
    pthread_mutex_unlock(&current_stratum_job_mutex);
}

void create_job_mining_notify(mining_notify * notifiy)
{
    pthread_mutex_lock(&current_stratum_job_mutex);
    if (current_job.job_id) {
        free(current_job.job_id);
    }

    if (current_job.coinbase_1) {
        free(current_job.coinbase_1);
    }

    if (current_job.coinbase_2) {
        free(current_job.coinbase_2);
    }

    // copy trivial types
    current_job = *notifiy;
    // duplicate dynamic strings with unknown length
    current_job.job_id = strdup(notifiy->job_id);
    current_job.coinbase_1 = strdup(notifiy->coinbase_1);
    current_job.coinbase_2 = strdup(notifiy->coinbase_2);
    pthread_mutex_unlock(&current_stratum_job_mutex);

    trigger_job_creation();
}

void * create_jobs_task(void * pvParameters)
{
    GlobalState * GLOBAL_STATE = (GlobalState *) pvParameters;

    ESP_LOGI(TAG, "ASIC Job Interval: %.2f ms", GLOBAL_STATE->asic_job_frequency_ms);
    SYSTEM_notify_mining_started(GLOBAL_STATE);
    ESP_LOGI(TAG, "ASIC Ready!");

    // Create the timer
    TimerHandle_t job_timer = xTimerCreate(TAG, pdMS_TO_TICKS(GLOBAL_STATE->asic_job_frequency_ms), pdTRUE, NULL, create_job_timer);

    if (job_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create timer");
        return NULL;
    }

    // Start the timer
    if (xTimerStart(job_timer, 0) != pdPASS) {
        ESP_LOGE(TAG, "Failed to start timer");
        return NULL;
    }

    // initialize notify
    memset(&current_job, 0, sizeof(mining_notify));

    uint32_t last_pool_diff = 0;
    uint32_t last_ntime = 0;
    uint64_t last_submit_time = 0;
    uint32_t extranonce_2 = 0;

    while (1) {
        pthread_mutex_lock(&job_mutex);
        pthread_cond_wait(&job_cond, &job_mutex); // Wait for the timer or external trigger
        pthread_mutex_unlock(&job_mutex);

        pthread_mutex_lock(&current_stratum_job_mutex);

        if (!current_job.ntime) {
            pthread_mutex_unlock(&current_stratum_job_mutex);
            continue;
        }

        if (last_ntime != current_job.ntime) {
            last_ntime = current_job.ntime;
            ESP_LOGI(TAG, "New Work Received %s", current_job.job_id);
        }

        char * extranonce_2_str = extranonce_2_generate(extranonce_2, extranonce_2_len);

        char * coinbase_tx =
            construct_coinbase_tx(current_job.coinbase_1, current_job.coinbase_2, extranonce_str, extranonce_2_str);

        char * merkle_root =
            calculate_merkle_root_hash(coinbase_tx, (uint8_t(*)[32]) current_job._merkle_branches, current_job.n_merkle_branches);

        bm_job * next_job = construct_bm_job(&current_job, merkle_root, version_mask);

        next_job->jobid = strdup(current_job.job_id);
        next_job->extranonce2 = strdup(extranonce_2_str);
        next_job->pool_diff = stratum_difficulty;

        pthread_mutex_unlock(&current_stratum_job_mutex);

        if (next_job->pool_diff != last_pool_diff) {
            ESP_LOGI(TAG, "New pool difficulty %lu", next_job->pool_diff);
            last_pool_diff = next_job->pool_diff;

            (*GLOBAL_STATE->ASIC_functions.set_difficulty_mask_fn)(next_job->pool_diff);
        }

        uint64_t current_time = esp_timer_get_time();
        if (last_submit_time) {
            ESP_LOGI(TAG, "job interval %dms", (int) ((current_time - last_submit_time) / 1e3));
        }
        last_submit_time = current_time;

        (*GLOBAL_STATE->ASIC_functions.send_work_fn)(GLOBAL_STATE, next_job);

        free(coinbase_tx);
        free(merkle_root);
        free(extranonce_2_str);
        extranonce_2++;
    }

    return NULL;
}
