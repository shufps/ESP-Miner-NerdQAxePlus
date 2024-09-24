#include <limits.h>
#include <pthread.h>
#include <sys/time.h>
#include <string.h>

#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "mining.h"

#include "global_state.h"

#include "system.h"
#include "boards/board.h"

static const char *TAG = "create_jobs_task";

pthread_mutex_t job_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t job_cond = PTHREAD_COND_INITIALIZER;

pthread_mutex_t current_stratum_job_mutex = PTHREAD_MUTEX_INITIALIZER;

static mining_notify current_job;

static char *extranonce_str = NULL;
static int extranonce_2_len = 0;

static uint32_t stratum_difficulty = 8192;
static uint32_t version_mask = 0;

#define min(a,b) ((a<b)?(a):(b))
#define max(a,b) ((a>b)?(a):(b))

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

void create_job_set_version_mask(uint32_t mask)
{
    pthread_mutex_lock(&current_stratum_job_mutex);
    version_mask = mask;
    pthread_mutex_unlock(&current_stratum_job_mutex);
}

bool create_job_set_difficulty(uint32_t diffituly)
{
    pthread_mutex_lock(&current_stratum_job_mutex);

    // new difficulty?
    bool is_new = stratum_difficulty != diffituly;

    // set difficulty
    stratum_difficulty = diffituly;
    pthread_mutex_unlock(&current_stratum_job_mutex);
    return is_new;
}

void create_job_set_enonce(char *enonce, int enonce2_len)
{
    pthread_mutex_lock(&current_stratum_job_mutex);
    if (extranonce_str) {
        free(extranonce_str);
    }
    extranonce_str = strdup(enonce);
    extranonce_2_len = enonce2_len;
    pthread_mutex_unlock(&current_stratum_job_mutex);
}

void create_job_mining_notify(mining_notify *notifiy)
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


void *create_jobs_task(void *pvParameters)
{
    ESP_LOGI(TAG, "ASIC Job Interval: %.2f ms", board.get_asic_job_frequency_ms());
    SYSTEM_notify_mining_started();
    ESP_LOGI(TAG, "ASIC Ready!");

    // Create the timer
    TimerHandle_t job_timer = xTimerCreate(TAG, pdMS_TO_TICKS(board.get_asic_job_frequency_ms()), pdTRUE, NULL, create_job_timer);

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

    uint32_t last_asic_diff = 0;
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

        // generate extranonce2 hex string
        char extranonce_2_str[extranonce_2_len * 2 + 1]; // +1 zero termination
        snprintf(extranonce_2_str, sizeof(extranonce_2_str), "%0*lx", (int) extranonce_2_len * 2, extranonce_2);

        // generate coinbase tx
        int coinbase_tx_len = strlen(current_job.coinbase_1) + strlen(extranonce_str) + strlen(extranonce_2_str) + strlen(current_job.coinbase_2);
        char coinbase_tx[coinbase_tx_len + 1]; // +1 zero termination
        snprintf(coinbase_tx, sizeof(coinbase_tx), "%s%s%s%s", current_job.coinbase_1, extranonce_str, extranonce_2_str,
                 current_job.coinbase_2);

        // calculate merkle root
        char merkle_root[65];
        calculate_merkle_root_hash(coinbase_tx, current_job._merkle_branches, current_job.n_merkle_branches, merkle_root);

        // we need malloc because we will save it in the job array
        bm_job *next_job = (bm_job *) malloc(sizeof(bm_job));
        construct_bm_job(&current_job, merkle_root, version_mask, next_job);

        next_job->jobid = strdup(current_job.job_id);
        next_job->extranonce2 = strdup(extranonce_2_str);
        next_job->pool_diff = stratum_difficulty;

        // clamp stratum difficulty
        next_job->asic_diff = max(min(stratum_difficulty, ASIC_MAX_DIFFICULTY), ASIC_MIN_DIFFICULTY);

        pthread_mutex_unlock(&current_stratum_job_mutex);

        if (next_job->asic_diff != last_asic_diff) {
            ESP_LOGI(TAG, "New ASIC difficulty %lu", next_job->asic_diff);
            last_asic_diff = next_job->asic_diff;

            board.asic_set_job_difficulty_mask(next_job->asic_diff);
        }

        uint64_t current_time = esp_timer_get_time();
        if (last_submit_time) {
            ESP_LOGI(TAG, "job interval %dms", (int) ((current_time - last_submit_time) / 1e3));
        }
        last_submit_time = current_time;

        int asic_job_id = board.asic_send_work(extranonce_2, next_job);

        ESP_LOGI(TAG, "Sent Job: %02X", asic_job_id);

        // save job
        pthread_mutex_lock(&ASIC_TASK_MODULE.valid_jobs_lock);
        // if a slot was used before free it
        if (ASIC_TASK_MODULE.active_jobs[asic_job_id] != NULL) {
            free_bm_job(ASIC_TASK_MODULE.active_jobs[asic_job_id]);
        }
        // save job into slot and set valid_job flag
        ASIC_TASK_MODULE.active_jobs[asic_job_id] = next_job;
        ASIC_TASK_MODULE.valid_jobs[asic_job_id] = 1;
        pthread_mutex_unlock(&ASIC_TASK_MODULE.valid_jobs_lock);

        extranonce_2++;
    }

    return NULL;
}
