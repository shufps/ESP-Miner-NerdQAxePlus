#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "can_sender.h"

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

/** Returns true if slave_id has completed CAN negotiation and is active. */
bool can_master_is_slave_active(uint8_t slave_id);

/** Returns true if slave_id is in the registry (may be inactive). */
bool can_master_is_slave_known(uint8_t slave_id);

/** Returns true if slave came from a different fleet. */
bool can_master_is_slave_foreign(uint8_t slave_id);

/** Copy MAC address for slave_id into out[6]. Returns false if not known. */
bool can_master_get_slave_mac(uint8_t slave_id, uint8_t out[6]);

/** Copy latest telemetry for slave_id. Returns false if not known. */
bool can_master_get_slave_telemetry(uint8_t slave_id, can_slave_telemetry_t *out);

/** Remove slave from registry and persist to NVS. */
void can_master_delete_slave(uint8_t slave_id);

/** Sum of power (W) reported by all active slaves. */
float can_master_get_slave_fleet_power(void);

#ifdef __cplusplus
}
#endif
