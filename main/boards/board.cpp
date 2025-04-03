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
}

void Board::loadSettings()
{
    m_fanPerc = Config::getFanSpeed();

    // default values are initialized in the constructor of each board
    m_asicFrequency = Config::getAsicFrequency(m_asicFrequency);
    m_asicVoltageMillis = Config::getAsicVoltage(m_asicVoltageMillis);
    m_asicJobIntervalMs = Config::getAsicJobInterval(m_asicJobIntervalMs);
    m_fanInvertPolarity = Config::isInvertFanPolarityEnabled(m_fanInvertPolarity);
    m_flipScreen = Config::isFlipScreenEnabled(m_flipScreen);

    ESP_LOGI(TAG, "ASIC Frequency: %dMHz", m_asicFrequency);
    ESP_LOGI(TAG, "ASIC voltage: %dmV", m_asicVoltageMillis);
    ESP_LOGI(TAG, "ASIC job interval: %dms", m_asicJobIntervalMs);
    ESP_LOGI(TAG, "invert fan polarity: %s", m_fanInvertPolarity ? "true" : "false");
    ESP_LOGI(TAG, "fan speed: %d%%", (int) m_fanPerc);
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
    vTaskDelay(1000 / portTICK_PERIOD_MS);

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

    float perc = (float) result / 100.0f;
    m_fanPerc = perc;
    setFanSpeed(perc);
    return result;
}

