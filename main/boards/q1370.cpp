#include "q1370.h"
#include "./drivers/EMC2302.h"
#include "board.h"
#include "esp_log.h"
#include "i2c_master.h"
#include "serial.h"

static const char *TAG = "q1370";

#define ASIC_RESET_EXP_PIN 0
#define VREG_EXP_PIN 1
#define LDO_EXP_PIN 2
#define CAN_SLAVE_EXP_PIN 5  // DIP switch: pulled to GND = slave mode, pull-up required

Q1370B::Q1370B() : NerdQaxePlus()
{
    m_deviceModel = "Q1370";
    m_miningAgent = m_deviceModel;
    m_asicModel = "BM1370";
    m_asicCount = 4;
    m_numPhases = 4;
    m_imax = 123;
    m_ifault = (float) 160.0f;

    m_asicJobIntervalMs = 500;
    m_asicFrequencies = {500, 515, 525, 550, 575, 590, 600};
    m_asicVoltages = {1120, 1130, 1140, 1150, 1160, 1170, 1180, 1190, 1200};
    m_defaultAsicFrequency = m_asicFrequency = 600;
    m_defaultAsicVoltageMillis = m_asicVoltageMillis = 1150; // default voltage
    m_absMaxAsicFrequency = 1200;
    m_absMaxAsicVoltageMillis = 1400;
    m_initVoltageMillis = 1200;

    m_pidSettings[0].targetTemp = 55;
    m_pidSettings[0].p = 600; //   6.00
    m_pidSettings[0].i = 10;  //   0.10
    m_pidSettings[0].d = 1000; // 10.00

    m_pidSettings[1].targetTemp = 65;  // target temp for vreg
    m_pidSettings[1].p = 600;  //   6.00
    m_pidSettings[1].i = 10;   //   0.10
    m_pidSettings[1].d = 1000; // 10.00

    m_maxPin = 150.0;
    m_minPin = 52.0;
    m_maxVin = 13.0;
    m_minVin = 11.0;
    m_minCurrentA = 0.0f;
    m_maxCurrentA = 20.0f;

    m_asicMaxDifficulty = 2048;
    m_asicMinDifficulty = 512;
    m_asicMinDifficultyDualPool = 256;

#ifdef Q1370
    m_theme = new ThemeGeneric();
#endif
    m_asics = new BM1370();
    m_hasHashCounter = true;
}

float Q1370B::getTemperature(int index)
{
    float temp = NerdQaxePlus::getTemperature(index);
    if (!temp) {
        return 0.0;
    }
    // we can't read the real chip temps but this should be about right
    return temp + 10.0f; // offset of 10°C
}

bool Q1370B::initBoard()
{
    Board::initBoard();

    SERIAL_init();

    // Init I2C
    if (i2c_master_init() != ESP_OK) {
        ESP_LOGE(TAG, "I2C initializing failed");
        return false;
    }

    // detect how many TMP1075 we have
    m_numTempSensors = detectNumTempSensors();

    ESP_LOGI(TAG, "found %d ASIC temp measuring sensors", m_numTempSensors);

    EMC2302_init(m_fanInvertPolarity);
    setFanSpeed(m_fanPerc);
    setFanSpeed(m_fanPerc);

    m_tmp451 = new Tmp451MuxExp(&m_io, 3, 4, 0x4c, true);
    m_hasTMux = m_tmp451->init() == ESP_OK;
#if 0
    if (!m_hasTMux) {
        ESP_LOGE(TAG, "TMUX probe failed. Assuming non-Q1370 board; applying safety limits.");

        // set new limits
        m_absMaxAsicVoltageMillis = 1150;
        m_absMaxAsicFrequency = 495;

        // reload settings to apply new absMax values
        loadSettings();
    }
#endif
    if (!m_io.init()) {
        ESP_LOGE(TAG, "FXL6408 failed init");
        return false;
    }

    // configure gpios i2c port extender
    m_io.set_direction(VREG_EXP_PIN, true);
    m_io.write(VREG_EXP_PIN, false);

    m_io.set_direction(LDO_EXP_PIN, true);
    m_io.write(LDO_EXP_PIN, false);

    m_io.set_direction(ASIC_RESET_EXP_PIN, true);
    m_io.write(ASIC_RESET_EXP_PIN, false);

    // CAN slave detect: pin 5 is input with pull-up.
    // DIP switch pulls to GND on slave boards; open = master.
    m_io.set_direction(CAN_SLAVE_EXP_PIN, false);
    m_io.enable_pull_up(CAN_SLAVE_EXP_PIN);

    return true;
}

void Q1370B::LDO_enable()
{
    m_io.write(LDO_EXP_PIN, true);
}
void Q1370B::LDO_disable()
{
    m_io.write(LDO_EXP_PIN, false);
}

void Q1370B::VREG_enable()
{
    m_io.write(VREG_EXP_PIN, true);
}

void Q1370B::VREG_disable()
{
    m_io.write(VREG_EXP_PIN, false);
}

void Q1370B::setAsicReset(bool state)
{
    m_io.write(ASIC_RESET_EXP_PIN, state);
}

bool Q1370B::isCanSlave()
{
    // Pin 5 is input with pull-up (configured in initBoard()).
    // DIP switch pulls to GND on slave → low = slave, high/open = master.
    bool level = true;
    if (m_io.read_pin(CAN_SLAVE_EXP_PIN, &level) != ESP_OK) {
        ESP_LOGE(TAG, "failed to read CAN slave detect pin");
        return false;
    }
    ESP_LOGI(TAG, "CAN slave detect pin: %d (%s)", level, level ? "master" : "slave");
    return !level;
}

void Q1370B::requestChipTemps() {
    // in shutdown the LDOs are not powered and we can't
    // measure the chip temps, so we reset it to 0 to prevent stale values
    if (m_shutdown) {
        for (int i=0;i<m_asicCount;i++) {
            setChipTemp(i, 0.0f);
        }
        return;
    }

    // don't try when we know we don't have it
    if (!m_hasTMux) {
        ESP_LOGE(TAG, "TMUX not detected.");
        return;
    }

    for (int i=0;i<m_asicCount;i++) {
        float temp = m_tmp451->get_temperature(i);
        // ESP_LOGI(TAG, "temperature of chip %d: %.3f", i, temp);
        if (!isnan(temp)) {
            setChipTemp(i, temp);
        }
    }
}
