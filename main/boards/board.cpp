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
    m_asicFrequency = nvs_config_get_u16(NVS_CONFIG_ASIC_FREQ, CONFIG_ASIC_FREQUENCY);
    m_asicVoltage = nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE, CONFIG_ASIC_VOLTAGE) / 1000.0f;
    m_fanInvertPolarity = nvs_config_get_u16(NVS_CONFIG_INVERT_FAN_POLARITY, 1);
    m_fanPerc = nvs_config_get_u16(NVS_CONFIG_FAN_SPEED, 100);

    // was initialized with board specific default value in the constructor
    m_asicJobIntervalMs = nvs_config_get_u16(NVS_CONFIG_ASIC_JOB_INTERVAL, m_asicJobIntervalMs);

    ESP_LOGI(TAG, "NVS_CONFIG_ASIC_FREQ %.3f", (float) m_asicFrequency);
    ESP_LOGI(TAG, "NVS_CONFIG_ASIC_VOLTAGE %.3f", (float) m_asicVoltage);
    ESP_LOGI(TAG, "NVS_CONFIG_ASIC_JOB_INTERVAL %d", (int) m_asicJobIntervalMs);
    ESP_LOGI(TAG, "NVS_CONFIG_INVERT_FAN_POLARITY %s", m_fanInvertPolarity ? "true" : "false");
    ESP_LOGI(TAG, "NVS_CONFIG_FAN_SPEED %d%%", (int) m_fanPerc);
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

double Board::getAsicJobIntervalMs()
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