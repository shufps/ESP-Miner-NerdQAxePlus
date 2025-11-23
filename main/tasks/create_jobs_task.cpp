#include <limits.h>
#include <pthread.h>
#include <string.h>
#include <sys/time.h>

#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "mining.h"

#include "global_state.h"

#include "boards/board.h"
#include "macros.h"
#include "system.h"

#define PRIMARY 0
#define SECONDARY 1

static const char *TAG = "create_jobs_task";

pthread_mutex_t job_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t job_cond = PTHREAD_COND_INITIALIZER;

pthread_mutex_t current_stratum_job_mutex = PTHREAD_MUTEX_INITIALIZER;

class MiningInfo {
  public:
    mining_notify *current_job = nullptr;

    char *extranonce_str = nullptr;
    int extranonce_2_len = 0;

    char *next_extranonce_str = nullptr;
    int next_extranonce_2_len = 0;

    uint32_t stratum_difficulty = 8192;
    uint32_t active_stratum_difficulty = 8192;
    uint32_t version_mask = 0;

  public:
    MiningInfo()
    {
        current_job = (mining_notify *) CALLOC(1, sizeof(mining_notify));
    }

    void set_version_mask(uint32_t mask)
    {
        version_mask = mask;
    }

    bool set_difficulty(uint32_t difficulty)
    {
        // new difficulty?
        bool is_new = stratum_difficulty != difficulty;

        // set difficulty
        stratum_difficulty = difficulty;
        return is_new;
    }

    void set_enonce(char *enonce, int enonce2_len)
    {
        safe_free(extranonce_str);

        extranonce_str = strdup(enonce);
        extranonce_2_len = enonce2_len;
    }

    void set_next_enonce(char *enonce, int enonce2_len)
    {
        safe_free(next_extranonce_str);

        next_extranonce_str = strdup(enonce);
        next_extranonce_2_len = enonce2_len;
    }

    void create_job_mining_notify(mining_notify *notify)
    {
        // do we have a pending extranonce switch?
        if (next_extranonce_str) {
            safe_free(extranonce_str);
            extranonce_str = strdup(next_extranonce_str);
            extranonce_2_len = next_extranonce_2_len;
            safe_free(next_extranonce_str);
            next_extranonce_2_len = 0;
        }

        safe_free(current_job->job_id);
        safe_free(current_job->coinbase_1);
        safe_free(current_job->coinbase_2);

        // copy trivial types
        memcpy(current_job, notify, sizeof(mining_notify));
        // duplicate dynamic strings with unknown length
        current_job->job_id = strdup(notify->job_id);
        current_job->coinbase_1 = strdup(notify->coinbase_1);
        current_job->coinbase_2 = strdup(notify->coinbase_2);

        // set active difficulty with the mining.notify command
        active_stratum_difficulty = stratum_difficulty;
    }

    void invalidate()
    {
        // mark as invalid
        current_job->ntime = 0;
        safe_free(extranonce_str);
        safe_free(next_extranonce_str);
        safe_free(current_job->job_id);
        safe_free(current_job->coinbase_1);
        safe_free(current_job->coinbase_2);
    }
};

MiningInfo miningInfo[2] = {MiningInfo{}, MiningInfo{}};

#define min(a, b) ((a < b) ? (a) : (b))
#define max(a, b) ((a > b) ? (a) : (b))

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

void create_job_set_version_mask(int pool, uint32_t mask)
{
    PThreadGuard g(current_stratum_job_mutex);
    miningInfo[pool].set_version_mask(mask);
}

bool create_job_set_difficulty(int pool, uint32_t difficulty)
{
    PThreadGuard g(current_stratum_job_mutex);
    return miningInfo[pool].set_difficulty(difficulty);
}

void create_job_set_enonce(int pool, char *enonce, int enonce2_len)
{
    PThreadGuard g(current_stratum_job_mutex);
    miningInfo[pool].set_enonce(enonce, enonce2_len);
}

void set_next_enonce(int pool, char *enonce, int enonce2_len)
{
    PThreadGuard g(current_stratum_job_mutex);
    miningInfo[pool].set_next_enonce(enonce, enonce2_len);
}

void create_job_mining_notify(int pool, mining_notify *notify, bool abandonWork)
{
    {
        PThreadGuard g(current_stratum_job_mutex);
        // clear jobs for pool
        if (abandonWork) {
            int deleted = asicJobs.cleanJobs(pool);
            ESP_LOGI(TAG, "%d jobs deleted from queue %d", deleted, pool);
        }
        miningInfo[pool].create_job_mining_notify(notify);
    }
    trigger_job_creation();
}

void create_job_invalidate(int pool)
{
    PThreadGuard g(current_stratum_job_mutex);
    miningInfo[pool].invalidate();
}

void *create_jobs_task(void *pvParameters)
{
    Board *board = SYSTEM_MODULE.getBoard();
    Asic *asics = board->getAsics();

    ESP_LOGI(TAG, "ASIC Job Interval: %d ms", board->getAsicJobIntervalMs());
    SYSTEM_MODULE.notifyMiningStarted();
    ESP_LOGI(TAG, "ASIC Ready!");

    // Create the timer
    TimerHandle_t job_timer = xTimerCreate(TAG, pdMS_TO_TICKS(board->getAsicJobIntervalMs()), pdTRUE, NULL, create_job_timer);

    if (job_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create timer");
        return NULL;
    }

    // Start the timer
    if (xTimerStart(job_timer, 0) != pdPASS) {
        ESP_LOGE(TAG, "Failed to start timer");
        return NULL;
    }

    uint32_t last_ntime[2]{0};
    uint64_t last_submit_time = 0;
    uint32_t extranonce_2 = 0;

    int lastJobInterval = board->getAsicJobIntervalMs();

    while (1) {
        if (POWER_MANAGEMENT_MODULE.isShutdown()) {
            ESP_LOGW(TAG, "suspended");
            vTaskSuspend(NULL);
        }
        pthread_mutex_lock(&job_mutex);
        pthread_cond_wait(&job_cond, &job_mutex); // Wait for the timer or external trigger
        pthread_mutex_unlock(&job_mutex);

        // job interval changed via UI
        if (board->getAsicJobIntervalMs() != lastJobInterval) {
            xTimerChangePeriod(job_timer, pdMS_TO_TICKS(board->getAsicJobIntervalMs()), 0);
            lastJobInterval = board->getAsicJobIntervalMs();
            continue;
        }

        bm_job *next_job = nullptr;
        int active_pool = 0;
        const char *active_pool_str = "";

        if (!STRATUM_MANAGER) {
            continue;
        }

        // select pool to mine for
        active_pool = STRATUM_MANAGER->getNextActivePool();
        active_pool_str = active_pool ? "Sec" : "Pri";

        { // scope for mutex
            PThreadGuard g(current_stratum_job_mutex);

            // set current pool data
            MiningInfo *mi = &miningInfo[active_pool];

            if (!mi->current_job->ntime || !asics) {
                continue;
            }

            if (last_ntime[active_pool] != mi->current_job->ntime) {
                last_ntime[active_pool] = mi->current_job->ntime;
                ESP_LOGI(TAG, "(%s) New Work Received %s", active_pool_str, mi->current_job->job_id);
            }

            // generate extranonce2 hex string
            char extranonce_2_str[mi->extranonce_2_len * 2 + 1]; // +1 zero termination
            snprintf(extranonce_2_str, sizeof(extranonce_2_str), "%0*lx", (int) mi->extranonce_2_len * 2, extranonce_2);

            // generate coinbase tx
            int coinbase_tx_len = strlen(mi->current_job->coinbase_1) + strlen(mi->extranonce_str) + strlen(extranonce_2_str) +
                                  strlen(mi->current_job->coinbase_2);
            char coinbase_tx[coinbase_tx_len + 1]; // +1 zero termination
            snprintf(coinbase_tx, sizeof(coinbase_tx), "%s%s%s%s", mi->current_job->coinbase_1, mi->extranonce_str,
                     extranonce_2_str, mi->current_job->coinbase_2);

            // calculate merkle root
            char merkle_root[65];
            calculate_merkle_root_hash(coinbase_tx, mi->current_job->_merkle_branches, mi->current_job->n_merkle_branches,
                                       merkle_root);

            // we need malloc because we will save it in the job array
            next_job = (bm_job *) malloc(sizeof(bm_job));
            construct_bm_job(mi->current_job, merkle_root, mi->version_mask, next_job);
            next_job->jobid = strdup(mi->current_job->job_id);
            next_job->extranonce2 = strdup(extranonce_2_str);
            next_job->pool_diff = mi->active_stratum_difficulty;
            next_job->pool_id = active_pool;
            next_job->asic_diff = STRATUM_MANAGER->selectAsicDiff(active_pool, mi->active_stratum_difficulty);
        } // mutex

        // set asic difficulty
        asics->setJobDifficultyMask(next_job->asic_diff);

        uint64_t current_time = esp_timer_get_time();
        if (last_submit_time) {
            ESP_LOGD(TAG, "(%s) job interval %dms", active_pool_str, (int) ((current_time - last_submit_time) / 1e3));
        }
        last_submit_time = current_time;

        int asic_job_id = asics->sendWork(extranonce_2, next_job);

        ESP_LOGD(TAG, "(%s) Sent Job (%d): %02X", active_pool_str, active_pool, asic_job_id);

        // save job
        asicJobs.storeJob(next_job, asic_job_id);

        extranonce_2++;
    }

    return NULL;
}
