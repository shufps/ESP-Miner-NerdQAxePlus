#include "board.h"
#include "nvs_config.h"
#include "esp_log.h"

const static char* TAG = "board";

Board::Board() {
    // NOP
}

void Board::load_settings()
{
    asic_frequency = nvs_config_get_u16(NVS_CONFIG_ASIC_FREQ, CONFIG_ASIC_FREQUENCY);
    asic_voltage = nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE, CONFIG_ASIC_VOLTAGE);
    fan_invert_polarity = nvs_config_get_u16(NVS_CONFIG_INVERT_FAN_POLARITY, 1);
    fan_perc = nvs_config_get_u16(NVS_CONFIG_FAN_SPEED, 100);

    ESP_LOGI(TAG, "NVS_CONFIG_ASIC_FREQ %.3f", (float) asic_frequency);
    ESP_LOGI(TAG, "NVS_CONFIG_ASIC_VOLTAGE %.3f", (float) asic_voltage / 1000.0f);
    ESP_LOGI(TAG, "NVS_CONFIG_INVERT_FAN_POLARITY %s", fan_invert_polarity ? "true" : "false");
    ESP_LOGI(TAG, "NVS_CONFIG_FAN_SPEED %d%%", (int) fan_perc);
}

const char *Board::get_device_model()
{
    return device_model;
}

int Board::get_version()
{
    return version;
}

const char *Board::get_asic_model()
{
    return asic_model;
}

int Board::get_asic_count()
{
    return asic_count;
}

double Board::get_asic_job_frequency_ms()
{
    return asic_job_frequency_ms;
}

