#pragma once

#include <stdint.h>
#include <stdbool.h>

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

/**
 * Returns true if slave_id has completed CAN negotiation and is active.
 * Thread-safe for read (single byte, written only by can_master_task).
 */
bool can_master_is_slave_active(uint8_t slave_id);

#ifdef __cplusplus
}
#endif
