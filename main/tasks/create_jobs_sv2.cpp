#include "create_jobs_sv2.h"
#include "create_jobs_task.h"
#include "mining_info_v2.h"
#include "global_state.h"
#include "macros.h"
#include "coinbase_decoder.h"
#include "mining_utils.h"
#include "nvs_config.h"

#include "esp_log.h"

static const char *TAG = "create_jobs_sv2";

/**
 * Process SV2 Extended Channel coinbase for block header display.
 * Converts binary coinbase_prefix/suffix to hex and runs coinbase_process().
 */
static void processSV2ExtendedCoinbase(int pool, const sv2_ext_job_t *job,
                                        const uint8_t *extranonce_prefix, uint8_t extranonce_prefix_len,
                                        uint8_t extranonce_size)
{
    if (!job->coinbase_prefix || !job->coinbase_suffix || job->coinbase_prefix_len == 0) return;
    if (!STRATUM_MANAGER) return;

    // Convert binary prefix/suffix to hex strings for coinbase_process()
    char *prefix_hex = (char *)malloc(job->coinbase_prefix_len * 2 + 1);
    char *suffix_hex = (char *)malloc(job->coinbase_suffix_len * 2 + 1);
    char *enonce_hex = (char *)malloc(extranonce_prefix_len * 2 + 1);
    if (!prefix_hex || !suffix_hex || !enonce_hex) {
        free(prefix_hex); free(suffix_hex); free(enonce_hex);
        return;
    }

    bin2hex(job->coinbase_prefix, job->coinbase_prefix_len, prefix_hex, job->coinbase_prefix_len * 2 + 1);
    bin2hex(job->coinbase_suffix, job->coinbase_suffix_len, suffix_hex, job->coinbase_suffix_len * 2 + 1);
    bin2hex(extranonce_prefix, extranonce_prefix_len, enonce_hex, extranonce_prefix_len * 2 + 1);

    // Get user address for payout matching (fee calculation)
    char *user = (pool == 0) ? Config::getStratumUser() : Config::getStratumFallbackUser();

    coinbase_result_t result{};
    esp_err_t err = coinbase_process(
        prefix_hex, suffix_hex,
        job->version, job->nbits,
        enonce_hex, extranonce_size,
        user, &result
    );

    safe_free(user);

    if (err == ESP_OK) {
        STRATUM_MANAGER->setCoinbaseResult(pool, result);
    }

    free(prefix_hex);
    free(suffix_hex);
    free(enonce_hex);
}

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

    // Decode coinbase for block header display
    processSV2ExtendedCoinbase(pool, job, extranonce_prefix, extranonce_prefix_len, extranonce_size);

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
