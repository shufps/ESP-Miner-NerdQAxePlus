#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Dynamically assigned CAN slave ID. CAN_SLAVE_ID_UNASSIGNED until negotiation completes. */
extern volatile uint8_t g_can_slave_id;

/**
 * CAN Slave RX task.
 * Receives raw BM1368_job frames, forwards to ASICs, returns nonces to master.
 * Must be started after can_init() and board->initAsics().
 */
void can_slave_task(void *pvParameters);

/**
 * CAN Slave ASIC result task.
 * Calls processWork() in a tight loop, handles register responses (temp/hashrate)
 * and sends real nonces back to master via CAN.
 * Must be started after can_init() and board->initAsics().
 */
void can_slave_result_task(void *pvParameters);

/**
 * CAN Slave telemetry task.
 * Periodically pushes can_slave_telemetry_t to master (~1/s).
 * Pass slave_id as pvParameters (cast from uint32_t).
 * Must be started after can_init() and board->initAsics().
 */
void can_slave_telemetry_task(void *pvParameters);

#ifdef __cplusplus
}
#endif
