#pragma once

#include <stdint.h>
#include "mining.h"

// CAN IDs
#define CAN_ID_JOB_BASE       0x200   // + slave_id, master→slave: raw ASIC job
#define CAN_ID_NONCE_BASE     0x300   // + slave_id, slave→master: found nonce
#define CAN_ID_TELEMETRY_BASE 0x400   // + slave_id, slave→master: telemetry
#define CAN_ID_SETTINGS_BASE  0x500   // + slave_id, master→slave: settings cmd

#define CAN_TELEMETRY_VERSION 1

// Telemetry payload pushed by each slave ~1/s.
// Fixed at 4 ASICs for MVP — extend asicTemps[] later if needed.
typedef struct __attribute__((__packed__)) {
    uint8_t  version;           // CAN_TELEMETRY_VERSION
    float    temp;              // max ASIC chip temp (°C)
    float    vrTemp;            // VReg temp (°C)
    float    asicTemps[4];      // per-chip temps (°C)
    uint16_t fanRpm;            // fan 0 RPM
    uint16_t fanRpm2;           // fan 1 RPM
    uint8_t  fanSpeed;          // fan 0 duty %
    uint8_t  fanSpeed2;         // fan 1 duty %
    float    power;             // Watt
    uint16_t current;           // mA
    uint16_t coreVoltageActual; // mV
    float    hashRate;          // GH/s (from HASHRATE_MONITOR)
    uint8_t  shutdown;          // 1 = thermal shutdown active
} can_slave_telemetry_t;        // 44 bytes → 7 CAN frames

// Multiframe SEQ byte: 0x00..0x7E = continuation, 0xFF = last frame
#define CAN_SEQ_LAST        0xFF

// Upper 7 bits of extranonce_2 encode the slave_id (0..127).
// Lower 25 bits are the per-slave rolling counter.
#define CAN_SLAVE_COUNT     2
#define CAN_ENONCE2_SLAVE_SHIFT 25

/**
 * Send telemetry from slave to master (~1/s).
 * Call from the slave-side periodic task.
 * slave_id is derived from the DIP switch / isCanSlave config.
 */
void can_send_telemetry(uint8_t slave_id, const can_slave_telemetry_t *t);

/**
 * Send a raw job to one slave over CAN.
 * Assembles the same BM1368_job byte layout that asic.cpp uses and
 * transmits it as multiframe CAN message. The slave forwards it 1:1 to its ASIC.
 *
 * @param slave_id  0..127
 * @param job_id    ASIC job ID byte (from jobToAsicId())
 * @param job       fully built bm_job for this slave (merkle_root already slave-specific)
 */
void can_send_raw_job(uint8_t slave_id, uint8_t job_id, const bm_job *job);

/**
 * Returns the extranonce_2 value to use for a given slave and counter.
 * extranonce_2 = (slave_id << 25) | (counter & 0x1FFFFFF)
 */
static inline uint32_t can_make_extranonce2(uint8_t slave_id, uint32_t counter)
{
    return ((uint32_t)(slave_id & 0x7F) << CAN_ENONCE2_SLAVE_SHIFT) | (counter & 0x01FFFFFF);
}
