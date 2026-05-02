#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * CAN Master receiver task.
 *
 * Listens for nonce frames from slaves (CAN ID 0x300 | slave_id),
 * reassembles the multiframe payload, looks up the originating bm_job
 * from asicJobs, validates the nonce and submits to Stratum — exactly
 * like ASIC_result_task does for local ASICs.
 */
void can_master_task(void *pvParameters);

#ifdef __cplusplus
}
#endif
