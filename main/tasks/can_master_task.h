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

/** Returns true if any slave is in thermal shutdown; fills *out_id with its ID. */
bool can_master_get_fleet_shutdown(uint8_t *out_id);

/**
 * Returns true if any slave has an active error (explicit shutdown OR board fault).
 * Fills *out_id with the slave ID and *out_board_error with the Board::Error value.
 */
bool can_master_get_fleet_error(uint8_t *out_id, uint8_t *out_board_error);

/** Send a SET_FREQ command to a slave (MHz). */
void can_master_set_slave_freq(uint8_t slave_id, uint16_t freq_mhz);

/** Send a SET_VOLTAGE command to a slave (mV). */
void can_master_set_slave_voltage(uint8_t slave_id, uint16_t mv);

/** Send a SET_FAN command to a slave. mode: 0=manual,2=pid,3=linked. */
void can_master_set_slave_fan(uint8_t slave_id, uint8_t ch, uint8_t mode,
                              uint8_t speed, uint8_t target_temp, uint8_t overheat);

/** Send a SET_DISPLAY command to a slave. */
void can_master_set_slave_display(uint8_t slave_id, uint8_t flip, uint8_t auto_off);

/** Send SHUTDOWN command — slave stops mining. */
void can_master_shutdown_slave(uint8_t slave_id);

/** Send IDENTIFY command — slave blinks display. */
void can_master_identify_slave(uint8_t slave_id);

/** Send RESTART command — slave saves NVS and reboots. */
void can_master_restart_slave(uint8_t slave_id);

#ifdef __cplusplus
}
#endif
