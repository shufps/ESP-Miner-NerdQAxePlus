#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "board.h"
#include "nvs_config.h"
#include "esp_log.h"
#include "../displays/displayDriver.h"

const static char* TAG = "board";

Board::Board() {
    // afc settings
    m_afcMinTemp = 45.0f;
    m_afcMinFanSpeed = 35.0f;
    m_afcMaxTemp = 65.0f;
    m_fanAutoPolarity = true; // default detect polarity
}

void Board::loadSettings()
{
    m_fanPerc = Config::getFanSpeed();

    // default values are initialized in the constructor of each board
    m_asicFrequency = Config::getAsicFrequency(m_asicFrequency);
    m_asicVoltageMillis = Config::getAsicVoltage(m_asicVoltageMillis);
    m_asicJobIntervalMs = Config::getAsicJobInterval(m_asicJobIntervalMs);
    m_fanInvertPolarity = Config::isInvertFanPolarityEnabled(m_fanInvertPolarity);
    m_fanAutoPolarity = Config::isAutoFanPolarityEnabled(m_fanAutoPolarity);
    m_flipScreen = Config::isFlipScreenEnabled(m_flipScreen);

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

// Adjust the fan speed based on chip temperature, scaling smoothly from m_afcMinFanSpeed up to 100%.
// The fan starts ramping up once the temperature exceeds m_afcMinTemp and reaches full speed at m_afcMaxTemp.
float Board::automaticFanSpeed(float temp)
{
    float result = 0.0;

    if (temp < m_afcMinTemp) {
        result = m_afcMinFanSpeed;
    } else if (temp >= m_afcMaxTemp) {
        result = 100;
    } else {
        float temp_range = m_afcMaxTemp - m_afcMinTemp;
        float fan_range = 100.0f - m_afcMinFanSpeed;
        result = ((temp - m_afcMinTemp) / temp_range) * fan_range + m_afcMinFanSpeed;
    }

    return result;
}

FanPolarityGuess Board::guessFanPolarity() {
    const int settleTimeMs = 2000;
    const float lowPWM = 0.40f;
    const float highPWM = 0.60f;
    const float similarityThreshold = 0.90f; // ≥90% match = too similar to tell

    uint16_t rpmLow = 0, rpmHigh = 0;

    // bring it to run at a safe setting
    ESP_LOGI("polarity", "set 50%%");
    setFanPolarity(false);
    setFanSpeed(0.5f);
    vTaskDelay(pdMS_TO_TICKS(settleTimeMs));

    // Test low speed
    setFanSpeed(lowPWM);
    vTaskDelay(pdMS_TO_TICKS(settleTimeMs));
    getFanSpeed(&rpmLow);
    ESP_LOGI("polarity", "set %.2f%% read: %d", lowPWM, rpmLow);

    // Test high speed
    setFanSpeed(highPWM);
    vTaskDelay(pdMS_TO_TICKS(settleTimeMs));
    getFanSpeed(&rpmHigh);
    ESP_LOGI("polarity", "set %.2f%% read: %d", highPWM, rpmHigh);

    // Reset to mid-range to be safe
    ESP_LOGI("polarity", "set 50%%");
    setFanSpeed(0.5f);

    // No signal at all? Can't tell.
    if (rpmLow == 0 && rpmHigh == 0) {
        ESP_LOGW("polarity", "unknown fan polarity!");
        return POLARITY_UNKNOWN;
    }

    // Calculate similarity
    uint16_t minRPM = std::min(rpmLow, rpmHigh);
    uint16_t maxRPM = std::max(rpmLow, rpmHigh);

    float similarity = (float)minRPM / (maxRPM + 1);  // avoid div by zero

    // Too close? Can't decide
    if (similarity > similarityThreshold) {
        ESP_LOGW("polarity", "RPM difference too little, unknown fan polarity!");
        return POLARITY_UNKNOWN;
    }

    // Now decide
    if (rpmHigh > rpmLow) {
        ESP_LOGI("polarity", "normal fan polarity detected");
        return POLARITY_NORMAL;
    } else {
        ESP_LOGI("polarity", "inverted fan polarity detected");
        return POLARITY_INVERTED;
    }
}