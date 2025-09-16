#include <string.h>

#include "esp_log.h"

#include "serial.h"
#include "utils.h"
#include "global_state.h"
#include "nvs_config.h"
#include "system.h"
#include "boards/board.h"

#include "simple_ring64.hpp"

static const char *TAG = "asic_result";

static SimpleRing64<32> s_seen_keys;

// Combine nonce + version into a single 64-bit key
static inline uint64_t make_key(uint32_t nonce, uint32_t version)
{
    // This order must be consistent everywhere
    return (uint64_t(nonce) << 32) | uint64_t(version);
}

void ASIC_result_task(void *pvParameters)
{
    Board* board = SYSTEM_MODULE.getBoard();
    Asic* asics = board->getAsics();

    while (1) {
        //ESP_LOGI("Memory", "%lu", esp_get_free_heap_size()); test
        task_result asic_result;

        // get the result
        if (!asics->processWork(&asic_result)) {
            continue;
        }

        if (asic_result.is_reg_resp) {
            switch (asic_result.reg) {
                case 0xb4: {
                    if (asic_result.data & 0x80000000) {
                        float ftemp = (float) (asic_result.data & 0x0000ffff) * 0.171342f - 299.5144f;;
                        ESP_LOGI(TAG, "asic %d temp: %.3f", (int) asic_result.asic_nr, ftemp);
                        board->setChipTemp(asic_result.asic_nr, ftemp);
                    }
                    break;
                }
                default: {
                    // NOP
                    break;
                }
            }
            continue;
        }

        uint8_t asic_job_id = asic_result.job_id;

        bm_job *job = asicJobs.getClone(asic_job_id);
        if (!job) {
            ESP_LOGI(TAG, "Invalid job id found, 0x%02X", asic_job_id);
            continue;
        }

        // now we have the original job and can `or` the version
        asic_result.rolled_version |= job->version;

        // check the nonce difficulty
        double nonce_diff = test_nonce_value(job, asic_result.nonce, asic_result.rolled_version);

        // get best known session diff
        char bestDiffString[16];
        System::suffixString(SYSTEM_MODULE.getBestSessionNonceDiff(), bestDiffString, sizeof(bestDiffString), 3);

        // log the ASIC response, including pool and best session difficulty using human-readable SI formatting
        ESP_LOGI(TAG, "Job ID: %02X AsicNr: %d Ver: %08" PRIX32 " Nonce %08" PRIX32 "; Extranonce2 %s diff %.1f/%lu/%s",
            asic_job_id, asic_result.asic_nr, asic_result.rolled_version, asic_result.nonce, job->extranonce2,
            nonce_diff, job->pool_diff, bestDiffString);

        uint64_t key = make_key(asic_result.nonce, asic_result.rolled_version);
        if (!s_seen_keys.insert_if_absent(key)) {
            ESP_LOGW(TAG, "duplicate share detected!");
            SYSTEM_MODULE.countDuplicateHWNonces();
        }

        if (nonce_diff > job->pool_diff) {
            STRATUM_MANAGER.submitShare(job->jobid, job->extranonce2, job->ntime, asic_result.nonce,
                                    asic_result.rolled_version ^ job->version);
        }

        if (nonce_diff > job->asic_diff) {
            SYSTEM_MODULE.notifyFoundNonce((double) job->asic_diff, asic_result.asic_nr);
        }

        SYSTEM_MODULE.checkForBestDiff(nonce_diff, job->target);

        free_bm_job(job);
    }
}
