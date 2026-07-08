#include "q1373.h"
#include "bm1373.h"
#include "./drivers/TPS53667.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_master.h"

static const char *TAG = "q1373";

// strap resistors on the FXL6408, read as inputs with pull-up:
// 0R to GND populated = low, not populated = high
#define PHASE_DETECT_EXP_PIN 6 // low = 6-phase VR (TPS53667), high = 4-phase (TPS53647)
#define ETH_DETECT_EXP_PIN 7   // low = no ethernet, high = ethernet populated

Q1373B::Q1373B() : Q1370B()
{
    m_deviceModel = "Q1373";
    m_miningAgent = m_deviceModel;
    m_asicModel = "BM1373";
    m_asicCount = 4;
    m_numPhases = 4;
    m_imax = 123;
    m_ifault = (float) 160.0f;

    m_maxPin = 180.0;
    m_minPin = 50.0;
    m_maxVin = 13.0;
    m_minVin = 11.0;
    m_minCurrentA = 0.0f;
    m_maxCurrentA = 15.0f;

    m_asicFrequencies = {250, 275, 300, 325, 350, 375, 400, 425, 475, 500, 550};
    m_asicVoltages = {980, 990, 1000, 1010, 1020, 1030, 1040, 1050, 1060, 1070, 1080};
    m_defaultAsicFrequency = m_asicFrequency = 350;
    m_defaultAsicVoltageMillis = m_asicVoltageMillis = 1010;
    m_absMaxAsicFrequency = 700;
    m_absMinAsicVoltageMillis = 900;
    m_absMaxAsicVoltageMillis = 1200;
    m_initVoltageMillis = 1050;

    m_pidSettings[0].targetTemp = 55;
    m_pidSettings[0].p = 600; //   6.00
    m_pidSettings[0].i = 10;  //   0.10
    m_pidSettings[0].d = 1000; // 10.00

    m_pidSettings[1].targetTemp = 65;  // target temp for vreg
    m_pidSettings[1].p = 600;  //   6.00
    m_pidSettings[1].i = 10;   //   0.10
    m_pidSettings[1].d = 1000; // 10.00

    m_asicMaxDifficulty = 4096;
    m_asicMinDifficulty = 1024;
    m_asicMinDifficultyDualPool = 512;

#ifdef Q1373
    m_theme = new ThemeGeneric();
#endif
    m_asics = new BM1373();

    // strap detection has to happen in the constructor because
    // hasEthernet() and loadSettings() are called before initBoard()
    detectBoardOptions();

    if (m_isTPS53667) {
        delete m_tps;
        m_tps = new TPS53667();

        if (m_is6Phase) {
            m_numPhases = 6;
            m_imax = 240; // R = 6000 / (num_phases * max_current) = 25K 0.1%
            m_ifault = 235.0f;
            m_maxPin = 270.0;
            m_maxCurrentA = 22.0f;
        } else {
            // BOM variant: TPS53667 with only 4 phases populated,
            // Rmon stays 25K (40A per phase)
            m_numPhases = 4;
            m_imax = 160;
            m_ifault = 155.0f;
        }
    } else if (m_is6Phase) {
        // 6 phases are only possible with the TPS53667
        ESP_LOGE(TAG, "6-phase strap set but no TPS53667 found; staying with 4-phase TPS53647");
    }
}

void Q1373B::detectBoardOptions()
{
    // the straps hang on the FXL6408, so I2C and the expander must be
    // brought up here already; initBoard() inits them again later
    if (i2c_master_init() != ESP_OK) {
        ESP_LOGE(TAG, "I2C init failed; keeping defaults (4 phases, ethernet)");
        return;
    }

    if (!m_io.init()) {
        ESP_LOGE(TAG, "FXL6408 init failed; keeping defaults (4 phases, ethernet)");
        return;
    }

    m_io.enable_pull_up(PHASE_DETECT_EXP_PIN);
    m_io.enable_pull_up(ETH_DETECT_EXP_PIN);

    // let the pins settle
    vTaskDelay(pdMS_TO_TICKS(1));

    bool level = true;
    if (m_io.read_pin(PHASE_DETECT_EXP_PIN, &level) == ESP_OK) {
        m_is6Phase = !level;
    } else {
        ESP_LOGE(TAG, "failed to read phase detect pin; assuming 4 phases");
    }

    if (m_io.read_pin(ETH_DETECT_EXP_PIN, &level) == ESP_OK) {
        m_hasEth = level;
    } else {
        ESP_LOGE(TAG, "failed to read ETH detect pin; assuming ethernet");
    }

    // detect the VR chip via its PMBus device code, independently of the
    // phase strap (the new board revision has a TPS53667 that BOM variants
    // can populate with only 4 phases)
    uint16_t device_code = 0;
    {
        TPS53647 probe;
        device_code = probe.get_device_code();
    }
    m_isTPS53667 = (device_code == TPS53667::DEVICE_CODE);

    ESP_LOGI(TAG, "detection: VR device code %04x (%s), %d phases, ethernet %s", device_code,
             m_isTPS53667 ? "TPS53667" : "TPS53647", m_is6Phase ? 6 : 4,
             m_hasEth ? "populated" : "not populated");
}

bool Q1373B::initBoard()
{
    if (!Q1370B::initBoard()) {
        return false;
    }

    if (m_tmp451) {
        m_tmp451->set_temp_cal(1.06f, -25.4f, -25.4f, -25.4f, -25.4f);
    }

    return true;
}
