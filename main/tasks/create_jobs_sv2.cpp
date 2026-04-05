#include "create_jobs_sv2.h"
#include "create_jobs_task.h"
#include "mining_info_v2.h"
#include "global_state.h"
#include "macros.h"

#include "esp_log.h"

static const char *TAG = "create_jobs_sv2";

// Thread-safe holders for V2 mining info (one per pool)
// These are allocated on first use and reused across jobs.
static MiningInfoV2Standard *s_v2_standard[2] = {nullptr, nullptr};
static MiningInfoV2Extended *s_v2_extended[2] = {nullptr, nullptr};

void create_job_sv2_standard(int pool, uint32_t job_id, uint32_t version,
                              const uint8_t merkle_root[32], const uint8_t prev_hash[32],
                              uint32_t ntime, uint32_t nbits,
                              uint32_t version_mask, uint32_t difficulty,
                              bool clean)
{
    PThreadGuard g(current_stratum_job_mutex);

    // Lazily allocate
    if (!s_v2_standard[pool]) {
        s_v2_standard[pool] = new MiningInfoV2Standard();
    }

    s_v2_standard[pool]->updateJob(job_id, version, merkle_root, prev_hash,
                                    ntime, nbits, version_mask, difficulty);

    // Clear old jobs on clean flag
    if (clean) {
        asicJobs.cleanJobs(pool);
    }

    // Point the global miningInfo to our V2 standard instance
    miningInfo[pool] = s_v2_standard[pool];

    ESP_LOGI(TAG, "(%s) SV2 standard job %lu ready, diff=%lu",
             pool ? "Sec" : "Pri", (unsigned long)job_id, (unsigned long)difficulty);

    trigger_job_creation();
}

void create_job_sv2_extended(int pool, const sv2_ext_job_t *job,
                              const uint8_t *extranonce_prefix, uint8_t extranonce_prefix_len,
                              uint8_t extranonce_size,
                              uint32_t version_mask, uint32_t difficulty,
                              bool clean)
{
    PThreadGuard g(current_stratum_job_mutex);

    // Lazily allocate
    if (!s_v2_extended[pool]) {
        s_v2_extended[pool] = new MiningInfoV2Extended();
    }

    s_v2_extended[pool]->updateJob(job, extranonce_prefix, extranonce_prefix_len,
                                    extranonce_size, version_mask, difficulty);

    // Clear old jobs on clean flag
    if (clean) {
        asicJobs.cleanJobs(pool);
    }

    // Point the global miningInfo to our V2 extended instance
    miningInfo[pool] = s_v2_extended[pool];

    ESP_LOGI(TAG, "(%s) SV2 extended job %lu ready, diff=%lu",
             pool ? "Sec" : "Pri", (unsigned long)job->job_id, (unsigned long)difficulty);

    trigger_job_creation();
}

void create_job_sv2_set_difficulty(int pool, uint32_t difficulty)
{
    PThreadGuard g(current_stratum_job_mutex);

    if (s_v2_standard[pool]) {
        s_v2_standard[pool]->setDifficulty(difficulty);
        ESP_LOGI(TAG, "(%s) SV2 standard difficulty updated to %lu",
                 pool ? "Sec" : "Pri", (unsigned long)difficulty);
        // No trigger - never resend Standard Channel jobs on SetTarget.
        // ASIC keeps mining, new difficulty applies to next pool job.
        return;
    }

    if (s_v2_extended[pool]) {
        s_v2_extended[pool]->setDifficulty(difficulty);
        ESP_LOGI(TAG, "(%s) SV2 extended difficulty updated to %lu",
                 pool ? "Sec" : "Pri", (unsigned long)difficulty);
        trigger_job_creation();
    }
}
