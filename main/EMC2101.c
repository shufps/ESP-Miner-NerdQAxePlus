#include "esp_log.h"
#include <stdio.h>

#include "EMC2101.h"

static const char * TAG = "EMC2101";

// static const char *TAG = "EMC2101.c";

// run this first. sets up the config register
bool EMC2101_init(bool invertPolarity)
{

    // set the TACH input
    esp_err_t ret = i2c_master_register_write_byte(EMC2101_I2CADDR_DEFAULT, EMC2101_REG_CONFIG, 0x04);
    if (ret != ESP_OK) {
        ESP_LOGE("EMC2101", "Failed to write 0x%02X to EMC2101 register 0x%02X, error: 0x%X", 0x04, EMC2101_REG_CONFIG, ret);
        return false;
    }

    ESP_LOGI("EMC2101", "Successfully wrote 0x%02X to EMC2101 register 0x%02X", 0x04, EMC2101_REG_CONFIG);
    

    if (invertPolarity) {
        ESP_ERROR_CHECK(i2c_master_register_write_byte(EMC2101_I2CADDR_DEFAULT, EMC2101_FAN_CONFIG, 0b00100011));
    }
    return true;
}

// takes a fan speed percent
void EMC2101_set_fan_speed(float percent)
{
    uint8_t speed;

    speed = (uint8_t) (63.0 * percent);
    ESP_ERROR_CHECK(i2c_master_register_write_byte(EMC2101_I2CADDR_DEFAULT, EMC2101_REG_FAN_SETTING, speed));
}

// RPM = 5400000/reading
uint16_t EMC2101_get_fan_speed(void)
{
    uint8_t tach_lsb, tach_msb;
    uint16_t reading;
    uint16_t RPM;

    ESP_ERROR_CHECK(i2c_master_register_read(EMC2101_I2CADDR_DEFAULT, EMC2101_TACH_LSB, &tach_lsb, 1));
    ESP_ERROR_CHECK(i2c_master_register_read(EMC2101_I2CADDR_DEFAULT, EMC2101_TACH_MSB, &tach_msb, 1));

    // ESP_LOGI(TAG, "Raw Fan Speed = %02X %02X", tach_msb, tach_lsb);

    reading = tach_lsb | (tach_msb << 8);
    RPM = 5400000 / reading;

    // ESP_LOGI(TAG, "Fan Speed = %d RPM", RPM);
    if (RPM == 82) {
        return 0;
    }
    return RPM;
}

float EMC2101_get_external_temp(void)
{
    uint8_t temp_msb, temp_lsb;
    uint16_t reading;

    ESP_ERROR_CHECK(i2c_master_register_read(EMC2101_I2CADDR_DEFAULT, EMC2101_EXTERNAL_TEMP_MSB, &temp_msb, 1));
    ESP_ERROR_CHECK(i2c_master_register_read(EMC2101_I2CADDR_DEFAULT, EMC2101_EXTERNAL_TEMP_LSB, &temp_lsb, 1));
    
    reading = temp_lsb | (temp_msb << 8);
    reading >>= 5;

    if (reading == EMC2101_TEMP_FAULT_OPEN_CIRCUIT) {
        ESP_LOGE(TAG, "EMC2101 TEMP_FAULT_OPEN_CIRCUIT");
    }
    if (reading == EMC2101_TEMP_FAULT_SHORT) {
        ESP_LOGE(TAG, "EMC2101 TEMP_FAULT_SHORT");
    }

    float result = (float) reading / 8.0;
    return result;
}

uint8_t EMC2101_get_internal_temp(void)
{
    uint8_t temp;
    ESP_ERROR_CHECK(i2c_master_register_read(EMC2101_I2CADDR_DEFAULT, EMC2101_INTERNAL_TEMP, &temp, 1));
    return temp;
}
