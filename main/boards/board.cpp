#include "board.h"
#include "nvs_config.h"
#include "esp_log.h"

const static char* TAG = "board";

Board::Board() {
    // NOP
}

void Board::loadSettings()
{
    m_asicFrequency = nvs_config_get_u16(NVS_CONFIG_ASIC_FREQ, CONFIG_ASIC_FREQUENCY);
    m_asicVoltage = nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE, CONFIG_ASIC_VOLTAGE);
    m_fanInvertPolarity = nvs_config_get_u16(NVS_CONFIG_INVERT_FAN_POLARITY, 1);
    m_fanPerc = nvs_config_get_u16(NVS_CONFIG_FAN_SPEED, 100);

    ESP_LOGI(TAG, "NVS_CONFIG_ASIC_FREQ %.3f", (float) m_asicFrequency);
    ESP_LOGI(TAG, "NVS_CONFIG_ASIC_VOLTAGE %.3f", (float) m_asicVoltage / 1000.0f);
    ESP_LOGI(TAG, "NVS_CONFIG_INVERT_FAN_POLARITY %s", m_fanInvertPolarity ? "true" : "false");
    ESP_LOGI(TAG, "NVS_CONFIG_FAN_SPEED %d%%", (int) m_fanPerc);
}

const char *Board::getDeviceModel()
{
    return m_deviceModel;
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

