#include <string.h>

#include "esp_log.h"

#include "serial.h"
#include "utils.h"
#include "global_state.h"
#include "nvs_config.h"
#include "system.h"
#include "boards/board.h"

static const char *TAG = "asic_result";

void ASIC_result_task(void *pvParameters)
{
    Board* board = SYSTEM_MODULE.getBoard();
    Asic* asics = board->getAsics();

    char *user = nvs_config_get_string(NVS_CONFIG_STRATUM_USER, STRATUM_USER);
    float asic_temp = 0.0f;
    int asic_count = board->getAsicCount() - 1;

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
                        float ftemp = (float) (asic_result.data & 0x0000ffff) * 0.171342f - 299.5144f;
                        int asic_nr = (int) asic_result.asic_nr;
                        ESP_LOGI(TAG, "asic %d temp: %.3f", asic_nr, ftemp);
                        if (ftemp > asic_temp) {
                            POWER_MANAGEMENT_MODULE.setAsicHighTemp(ftemp);
                            asic_temp = ftemp;                                         
                        }
                        if ( asic_count == asic_nr ){
                                asic_temp = 0.0f;                                
                        }
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

        // log the ASIC response
        ESP_LOGI(TAG, "Job ID: %02X AsicNr: %d Ver: %08" PRIX32 " Nonce %08" PRIX32 " diff %.1f of %ld.", asic_job_id,
                 asic_result.asic_nr, asic_result.rolled_version, asic_result.nonce, nonce_diff, job->asic_diff);

        if (nonce_diff > job->pool_diff) {
            STRATUM_V1_submit_share(STRATUM_TASK.getStratumSock(), user, job->jobid, job->extranonce2, job->ntime, asic_result.nonce,
                                    asic_result.rolled_version ^ job->version);
        }

        if (nonce_diff > job->asic_diff) {
            SYSTEM_MODULE.notifyFoundNonce((double) job->asic_diff, asic_result.asic_nr);
        }

        SYSTEM_MODULE.checkForBestDiff(nonce_diff, job->target);

        free_bm_job(job);
    }
}
