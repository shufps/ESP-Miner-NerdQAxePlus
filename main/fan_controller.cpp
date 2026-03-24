#include "fan_controller.h"

#include <algorithm>
#include <math.h>

#include "esp_log.h"
#include "nvs_config.h"

static const char* TAG = "fan_ctrl";

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

void FanController::applyConfig(int ch)
{
    if (!m_pid[ch]) return;

    float p = m_config[ch].pid.p / 100.0f;
    float i = m_config[ch].pid.i / 100.0f;
    float d = m_config[ch].pid.d / 100.0f;

    m_pid[ch]->SetTunings(p, i, d);
    m_pid[ch]->SetTarget(static_cast<float>(m_config[ch].pid.targetTemp));

    ESP_LOGI(TAG, "ch%d PID: target=%.1f°C p=%.2f i=%.2f d=%.2f",
             ch, static_cast<float>(m_config[ch].pid.targetTemp), p, i, d);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void FanController::init(Board* board, int sampleTimeMs)
{
    m_board        = board;
    m_sampleTimeMs = sampleTimeMs;
    m_numChannels  = std::min(board->getNumFans(), MAX_FANS);

    ESP_LOGI(TAG, "initialising for %d fan channel(s)", m_numChannels);

    // Load config from NVS (PID instances don't exist yet, applyConfig is skipped)
    loadSettings();

    // Create PID instances using the loaded config
    for (int ch = 0; ch < m_numChannels; ch++) {
        m_pidTarget[ch] = static_cast<float>(m_config[ch].pid.targetTemp);

        float p = m_config[ch].pid.p / 100.0f;
        float i = m_config[ch].pid.i / 100.0f;
        float d = m_config[ch].pid.d / 100.0f;

        m_pid[ch] = new PID(&m_pidInput[ch], &m_pidOutput[ch], &m_pidTarget[ch],
                            p, i, d, PID_P_ON_E, PID_DIRECT);
        m_pid[ch]->SetSampleTime(sampleTimeMs);
        m_pid[ch]->SetOutputLimits(15, 100);
        m_pid[ch]->SetMode(PID_AUTOMATIC);
        m_pid[ch]->SetControllerDirection(PID_REVERSE);
        m_pid[ch]->Initialize();

        ESP_LOGI(TAG, "ch%d: mode=%d manual=%d%% overheat=%d°C pid-target=%d°C p=%.2f i=%.2f d=%.2f",
                 ch, static_cast<int>(m_config[ch].mode),
                 m_config[ch].manualSpeed, m_config[ch].overheatTemp,
                 m_config[ch].pid.targetTemp, p, i, d);
    }
}

void FanController::loadSettings()
{
    for (int ch = 0; ch < m_numChannels; ch++) {
        PidSettings* bp = m_board->getPidSettings(ch);

        m_config[ch].mode         = static_cast<Mode>(Config::getFanMode(ch));
        m_config[ch].manualSpeed  = Config::getFanManualSpeed(ch);
        m_config[ch].overheatTemp = Config::getFanOverheatTemp(ch);
        m_config[ch].pid.targetTemp = Config::getFanPidTargetTemp(ch, bp->targetTemp);
        m_config[ch].pid.p          = Config::getFanPidP(ch, bp->p);
        m_config[ch].pid.i          = Config::getFanPidI(ch, bp->i);
        m_config[ch].pid.d          = Config::getFanPidD(ch, bp->d);

        applyConfig(ch);  // no-op if PID not yet created
    }
}

void FanController::update(float chipTempMax, float vrTemp)
{
    // Temperature input per channel: ch0=chip, ch1=VR
    float tempInput[MAX_FANS] = { chipTempMax, vrTemp };

    // only 2nd channel can be linked
    if (m_config[1].mode == Mode::LINKED) {
        tempInput[0] = fmaxf(chipTempMax, vrTemp);
    }

    for (int ch = 0; ch < m_numChannels; ch++) {
        // Read current RPM
        m_board->getFanSpeedCh(ch, &m_fanRPM[ch]);

        // Always compute PID for bumpless transfer when switching modes
        m_pidInput[ch] = tempInput[ch];
        if (m_pid[ch]) {
            m_pid[ch]->Compute();
        }

        // Overheat: drive fan to 100% and flag it (checked even in LINKED mode for shutdown purposes)
        if (m_config[ch].overheatTemp && tempInput[ch] > m_config[ch].overheatTemp) {
            m_overheated[ch] = true;
            m_fanPerc[ch]    = 100;
            m_board->setFanSpeedCh(ch, 1.0f);
            continue;
        }
        m_overheated[ch] = false;

        // LINKED: mirror channel-0 output (ch0 must be processed first)
        if (m_config[ch].mode == Mode::LINKED && ch > 0) {
            m_fanPerc[ch] = m_fanPerc[0];
            m_board->setFanSpeedCh(ch, static_cast<float>(m_fanPerc[ch]) / 100.0f);
            continue;
        }
        m_overheated[ch] = false;

        // Normal control
        switch (m_config[ch].mode) {
        case Mode::MANUAL:
            m_fanPerc[ch] = m_config[ch].manualSpeed;
            break;
        case Mode::PID:
            m_fanPerc[ch] = static_cast<uint16_t>(roundf(m_pidOutput[ch]));
            break;
        default:
            ESP_LOGE(TAG, "ch%d: unknown mode %d, defaulting to 100%%", ch, static_cast<int>(m_config[ch].mode));
            m_fanPerc[ch] = 100;
            break;
        }

        m_board->setFanSpeedCh(ch, static_cast<float>(m_fanPerc[ch]) / 100.0f);
    }
}

uint16_t FanController::getRPM(int ch) const
{
    if (ch < 0 || ch >= m_numChannels) return 0;
    return m_fanRPM[ch];
}

uint16_t FanController::getSpeedPerc(int ch) const
{
    if (ch < 0 || ch >= m_numChannels) return 0;
    return m_fanPerc[ch];
}

bool FanController::isOverheated(int ch) const
{
    if (ch < 0 || ch >= m_numChannels) return false;
    return m_overheated[ch];
}

uint16_t FanController::getOverheatTemp(int ch) const
{
    if (ch < 0 || ch >= m_numChannels) return 0;
    return m_config[ch].overheatTemp;
}
