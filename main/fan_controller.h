#pragma once

#include <stdint.h>
#include "boards/board.h"
#include "pid/PID_v1_bc.h"

/**
 * FanController manages up to two independent fan channels with separate PID
 * controllers, manual modes, and per-channel settings.
 *
 * Channel 0: PID input = chip/ASIC temperature (chipTempMax)
 * Channel 1: PID input = VR temperature (vrTemp), can be linked to channel 0
 *
 * update() must be called from inside the PowerManagementTask hardware lock
 * so that I2C fan access is serialised correctly.
 */
class FanController {
public:
    static constexpr int MAX_FANS = 2;

    enum class Mode : uint8_t {
        MANUAL = 0,
        PID    = 2,
        LINKED = 3,  // channel mirrors channel 0 speed (backwards-compat default for ch1)
    };

    struct ChannelConfig {
        Mode     mode         = Mode::LINKED;
        uint16_t manualSpeed  = 100;  // 0–100 %
        uint16_t overheatTemp = 70;   // °C; 0 = disabled; fan goes to 100% when exceeded
        PidSettings pid       = {60, 600, 10, 1000};  // targetTemp, p×100, i×100, d×100
    };

    /**
     * Initialise the controller. Must be called once before update().
     * Reads settings from NVS, allocates PID instances.
     * @param board       Board instance (used to query getNumFans, set/get fan speed)
     * @param sampleTimeMs  PID sample interval in ms (should match the loop period)
     */
    void init(Board* board, int sampleTimeMs);

    /**
     * Execute one control cycle. Call this inside the PowerManagementTask loop,
     * under the hardware lock.
     * @param chipTempMax  Maximum ASIC chip temperature (drives channel-0 PID)
     * @param vrTemp       Voltage regulator temperature (drives channel-1 PID)
     */
    void update(float chipTempMax, float vrTemp);

    /**
     * Reload all channel configs from NVS. Safe to call at runtime (does not
     * re-initialise the PID, only updates tunings/target for bumpless transfer).
     * Called from init() and from the HTTP settings handler after a PATCH.
     */
    void loadSettings();

    /** Current measured RPM for the given channel (0-indexed). */
    uint16_t getRPM(int ch) const;

    /** Current fan speed output in percent (0–100) for the given channel. */
    uint16_t getSpeedPerc(int ch) const;

    /**
     * True if the monitored temperature for the given channel exceeded its
     * configured overheatTemp threshold (and overheatTemp != 0).
     */
    bool isOverheated(int ch) const;

    /** Configured overheat threshold in °C for the given channel (0 = disabled). */
    uint16_t getOverheatTemp(int ch) const;

private:
    Board* m_board        = nullptr;
    int    m_numChannels  = 0;
    int    m_sampleTimeMs = 2000;

    ChannelConfig m_config[MAX_FANS];

    // PID state arrays – PID holds raw pointers into these
    float m_pidInput[MAX_FANS]  = {};
    float m_pidOutput[MAX_FANS] = {};
    float m_pidTarget[MAX_FANS] = {};
    PID*  m_pid[MAX_FANS]       = {};

    uint16_t m_fanRPM[MAX_FANS]    = {};
    uint16_t m_fanPerc[MAX_FANS]   = {};
    bool     m_overheated[MAX_FANS] = {};

    /** Push current config tunings and target into the PID instance for channel ch. */
    void applyConfig(int ch);
};
