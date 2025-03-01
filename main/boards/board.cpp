#include "board.h"
#include "nvs_config.h"
#include "esp_log.h"
#include "../displays/displayDriver.h"

const static char* TAG = "board";

Board::Board() {
    // NOP
}

void Board::loadSettings()
{
    m_fanInvertPolarity = Config::isInvertFanPolarityEnabled();
    m_fanPerc = Config::getFanSpeed();

    // the variables were initialized with board specific default values in the constructor
    // if we have settings in the NVS then we use it
    uint16_t nvsAsicFrequency = Config::getAsicFrequency();
    uint16_t nvsAsicVoltage = Config::getAsicVoltage();
    uint16_t nvsAsicJobInterval = Config::getAsicJobInterval();

    m_asicFrequency = nvsAsicFrequency ? nvsAsicFrequency : m_asicFrequency;
    m_asicVoltageMillis = nvsAsicVoltage ? nvsAsicVoltage : m_asicVoltageMillis;
    m_asicJobIntervalMs = nvsAsicJobInterval ? nvsAsicJobInterval : m_asicJobIntervalMs;

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
