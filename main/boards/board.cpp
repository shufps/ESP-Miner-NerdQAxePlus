#include <algorithm>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "board.h"
#include "nvs_config.h"
#include "esp_log.h"
#include "../displays/displayDriver.h"

const static char* TAG = "board";

Board::Board() {
    m_absMaxAsicFrequency = 0;
    m_absMaxAsicVoltageMillis = 0;
    m_vrFrequency = m_defaultVrFrequency = 0;
    m_hasHashCounter = false;
    m_ecoAsicFrequency = 0;
    m_ecoAsicVoltageMillis = 0;
    m_numFans = 1;
}

void Board::loadSettings()
{
    m_fanPerc = Config::getFanSpeed();

    // default values are initialized in the constructor of each board

    // clamp frequency and voltage to absMax values
    if (m_absMaxAsicFrequency) {
        m_asicFrequency = std::min((int) Config::getAsicFrequency(m_asicFrequency), m_absMaxAsicFrequency);
    } else {
        m_asicFrequency = (int) Config::getAsicFrequency(m_asicFrequency);
    }

    if (m_absMaxAsicVoltageMillis) {
        m_asicVoltageMillis = std::min((int) Config::getAsicVoltage(m_asicVoltageMillis), m_absMaxAsicVoltageMillis);
    } else {
        m_asicVoltageMillis = (int) Config::getAsicVoltage(m_asicVoltageMillis);
    }

    m_asicJobIntervalMs = Config::getAsicJobInterval(m_asicJobIntervalMs);
    m_fanInvertPolarity = Config::isInvertFanPolarityEnabled(m_fanInvertPolarity);
    m_flipScreen = Config::isFlipScreenEnabled(m_flipScreen);
    m_vrFrequency = Config::getVrFrequency(m_defaultVrFrequency);

    m_pidSettings.targetTemp = Config::getPidTargetTemp(m_pidSettings.targetTemp);
    m_pidSettings.p = Config::getPidP(m_pidSettings.p);
    m_pidSettings.i = Config::getPidI(m_pidSettings.i);
    m_pidSettings.d = Config::getPidD(m_pidSettings.d);

    ESP_LOGI(TAG, "ASIC Frequency: %dMHz", m_asicFrequency);
    ESP_LOGI(TAG, "ASIC voltage: %dmV", m_asicVoltageMillis);
    ESP_LOGI(TAG, "ASIC job interval: %dms", m_asicJobIntervalMs);
    ESP_LOGI(TAG, "invert fan polarity: %s", m_fanInvertPolarity ? "true" : "false");
    ESP_LOGI(TAG, "fan speed: %d%%", (int) m_fanPerc);
}

bool Board::initBoard() {
    m_chipTemps = new float[m_asicCount]();
    return true;
}

void Board::setChipTemp(int nr, float temp) {
    if (nr < 0 || nr >= m_asicCount) {
        return;
    }
    m_chipTemps[nr] = temp;
}

float Board::getChipTemp(int nr) {
    if (nr < 0 || nr >= m_asicCount) {
        return 0.0f;
    }
    return m_chipTemps[nr];
}

void Board::requestChipTemps() {
    // NOP
}

float Board::getMaxChipTemp() {
    float maxTemp = 0.0f;
    for (int i=0;i<m_asicCount;i++) {
        maxTemp = std::max(maxTemp, m_chipTemps[i]);
    }
    return maxTemp;
}

const char *Board::getDeviceModel()
{
    return m_deviceModel;
}

const char *Board::getMiningAgent()
{
    return m_miningAgent;
}

int Board::getVersion()
{
    return m_version;
}

const char *Board::getAsicModel()
{
    return m_asicModel;
}

int Board::getAsicCount()
{
    return m_asicCount;
}

int Board::getAsicJobIntervalMs()
{
    return m_asicJobIntervalMs;
}

bool Board::selfTest(){

    // Initialize the display
    DisplayDriver *temp_display;
    temp_display = new DisplayDriver();
    temp_display->init(this);

    temp_display->logMessage("Self test not supported on this board...");
    ESP_LOGI("board", "Self test not supported on this board");
    vTaskDelay(pdMS_TO_TICKS(1000));

    return false;
}

// requires loadSettings to update the variables
bool Board::setAsicFrequency(float frequency) {
    if (!validateFrequency(frequency)) {
        return false;
    }

    // not initialized
    if (!m_asics) {
        return false;
    }

    return m_asics->setAsicFrequency(frequency);
}

// set and get version rolling frequency
// requires loadSettings to update the variables
void Board::setVrFrequency(uint32_t freq) {
    if (!m_asics) {
        return;
    }
    m_asics->setVrFrequency(freq);
}

bool Board::validateVoltage(float core_voltage) {
    int millis = (int) (core_voltage * 1000.0f);
    // we allow m_absMaxAsicVoltageMillis = 0 for no limit to not break what was
    // working before on nerdaxe and nerdaxegamma
    if (m_absMaxAsicVoltageMillis && millis > m_absMaxAsicVoltageMillis) {
        ESP_LOGE(TAG, "Validation error. ASIC voltage %d is higher than absolute maximum value %d", millis, m_absMaxAsicVoltageMillis);
        return false;
    }
    return true;
}

bool Board::validateFrequency(float frequency) {
    // we allow m_absMaxAsicFrequency = 0 for no limit to not break what was
    // working before on nerdaxe and nerdaxegamma
    if (m_absMaxAsicFrequency && frequency > (float) m_absMaxAsicFrequency) {
        ESP_LOGE(TAG, "Validation error. ASIC Frequency %.3f is higher than absolute maximum value %.3f", frequency, (float) m_absMaxAsicFrequency);
        return false;
    }
    return true;
}
