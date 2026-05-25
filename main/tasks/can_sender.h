#pragma once

#include <stdint.h>
#include "mining.h"

// CAN IDs — negotiation range (0x100)
#define CAN_ID_HELLO          0x100   // slave broadcast: MAC[6]
#define CAN_ID_ASSIGN         0x101   // master broadcast: MAC[6] + can_id[1]
#define CAN_ID_MASTER_BOOT    0x102   // master broadcast: no payload

// CAN IDs — operational range
#define CAN_ID_JOB_BASE       0x200   // + slave_id, master→slave: raw ASIC job
#define CAN_ID_NONCE_BASE     0x300   // + slave_id, slave→master: found nonce
#define CAN_ID_TELEMETRY_BASE 0x400   // + slave_id, slave→master: live telemetry
#define CAN_ID_SETTINGS_BASE  0x500   // + slave_id, master→slave: settings cmd
#define CAN_ID_CONFIG_BASE    0x700   // + slave_id, slave→master: device info + settings (sent once after ASSIGN)

#define CAN_SLAVE_ID_UNASSIGNED 0xFF

#define CAN_TELEMETRY_VERSION 5

// Live telemetry pushed by each slave ~1/s. Settings removed — see can_slave_config_t.
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
    uint8_t  shutdown;          // 1 = explicit manual shutdown active
    uint8_t  boardError;        // Board::Error enum value (0 = NONE)
    uint32_t freeHeap;          // free internal RAM (bytes)
} can_slave_telemetry_t;        // 49 bytes → 7 CAN frames

// Device info + settings, sent once after ASSIGN and after any CMD_SET_*.
// Also re-sent on CMD_GET_CONFIG request.
typedef struct __attribute__((__packed__)) {
    char     deviceModel[16];   // board->getDeviceModel()
    char     fwVersion[16];     // esp_app_get_description()->version
    uint16_t freqMhz;           // ASIC frequency (MHz)
    uint16_t voltageMv;         // ASIC voltage (mV)
    uint8_t  fan0Mode;          // fan ch0 mode (0=manual,2=pid,3=linked)
    uint8_t  fan0Speed;         // fan ch0 manual duty %
    uint8_t  fan0TargetTemp;    // fan ch0 PID target °C
    uint8_t  fan0Overheat;      // fan ch0 shutdown temp °C
    uint8_t  fan1Mode;          // fan ch1 mode
    uint8_t  fan1Speed;         // fan ch1 manual duty %
    uint8_t  fan1TargetTemp;    // fan ch1 target °C
    uint8_t  fan1Overheat;      // fan ch1 shutdown temp °C
    uint8_t  flipScreen;        // 1 = display flipped
    uint8_t  autoScreenOff;     // 1 = auto screen off enabled
} can_slave_config_t;           // 46 bytes → 7 CAN frames

// Settings commands sent master→slave on CAN_ID_SETTINGS_BASE + slave_id.
// All are single-frame: SEQ=0xFF, byte[1]=cmd, bytes[2..] = value.
#define CAN_CMD_SET_FREQ     0x01  // uint16_le freq_mhz
#define CAN_CMD_SET_VOLTAGE  0x02  // uint16_le voltage_mv
#define CAN_CMD_SET_FAN      0x03  // ch(1)+mode(1)+speed(1)+target_temp(1)+overheat(1)
#define CAN_CMD_SET_DISPLAY  0x04  // flip(1)+auto_off(1)
#define CAN_CMD_GET_CONFIG   0x05  // no payload — slave replies with can_slave_config_t on CAN_ID_CONFIG_BASE
#define CAN_CMD_SHUTDOWN     0xFD  // no payload — slave triggers thermal shutdown
#define CAN_CMD_IDENTIFY     0xFE  // no payload — slave blinks display
#define CAN_CMD_RESTART      0xFF  // no payload — slave saves NVS + restarts

// Multiframe SEQ byte: 0x00..0x7E = continuation, 0xFF = last frame
#define CAN_SEQ_LAST        0xFF

// Upper 7 bits of extranonce_2 encode the slave_id (0..127).
// Lower 25 bits are the per-slave rolling counter.
#define CAN_SLAVE_MAX       32   // maximum number of slaves on the bus
#define CAN_ENONCE2_SLAVE_SHIFT 25

/**
 * Send telemetry from slave to master (~1/s).
 * Call from the slave-side periodic task.
 * slave_id is derived from the DIP switch / isCanSlave config.
 */
/** Slave broadcasts its MAC address during negotiation (6 bytes, single frame). */
void can_send_hello(const uint8_t mac[6]);

/** Master assigns a CAN ID to a slave identified by MAC. */
void can_send_assign(const uint8_t mac[6], uint8_t can_id);

/** Master announces a reboot with its own MAC so slaves can detect fleet changes. */
void can_send_master_boot(const uint8_t master_mac[6]);

void can_send_telemetry(uint8_t slave_id, const can_slave_telemetry_t *t);
void can_send_config(uint8_t slave_id, const can_slave_config_t *c);

/**
 * Send a settings command from master to one slave.
 * payload: cmd byte + value bytes (≤6 bytes total, fits in single CAN frame).
 */
void can_send_settings_cmd(uint8_t slave_id, const uint8_t *payload, size_t len);

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
