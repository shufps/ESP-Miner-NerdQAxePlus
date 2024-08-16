#include "esp_log.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "EMC2302.h"

const char *TAG = "emc2301";

void EMC2302_set_fan_speed(float percent) {
    int value = (int) (percent * 255.0 + 0.5);
    value = (value > 255) ? 255 : value;

    ESP_LOGI(TAG, "setting fan speed to %.2f%% (0x%02x)", percent, value);

    ESP_ERROR_CHECK(i2c_master_register_write_byte(EMC2302_ADDR, EMC2302_FAN1 + EMC2302_OFS_FAN_SETTING, (uint8_t) value));
    ESP_ERROR_CHECK(i2c_master_register_write_byte(EMC2302_ADDR, EMC2302_FAN2 + EMC2302_OFS_FAN_SETTING, (uint8_t) value));

}

uint16_t EMC2302_get_fan_speed(void) {
    uint8_t tach_lsb, tach_msb;


    // report only first fan
    ESP_ERROR_CHECK(i2c_master_register_read(EMC2302_ADDR, EMC2302_FAN1 + EMC2302_OFS_TACH_READING_LSB, &tach_lsb, 1));
    ESP_ERROR_CHECK(i2c_master_register_read(EMC2302_ADDR, EMC2302_FAN1 + EMC2302_OFS_TACH_READING_MSB, &tach_msb, 1));

    // ESP_LOGI(TAG, "Raw Fan Speed = %02X %02X", tach_msb, tach_lsb);

    uint16_t rpm = tach_lsb | (tach_msb << 8);
    ESP_LOGI(TAG, "fan speed: %d", rpm);
    return rpm;
}

bool EMC2302_init(bool invertPolarity) {
    ESP_LOGI(TAG, "initializing EMC2302");

    //ESP_ERROR_CHECK(i2c_master_register_write_byte(EMC2302_ADDR, EMC2302_CONFIG, 0x40 /* default */));

    // set polarity of ch1 and ch2
    ESP_ERROR_CHECK(i2c_master_register_write_byte(EMC2302_ADDR, EMC2302_POLARITY, (invertPolarity) ? 0x03 : 0x00));

    // set output type to push pull of ch1 and ch2
    ESP_ERROR_CHECK(i2c_master_register_write_byte(EMC2302_ADDR, EMC2302_OUTPUT_CONFIG, 0x03));

    // set base frequency of ch1 and ch2 to 19.53kHz
    ESP_ERROR_CHECK(i2c_master_register_write_byte(EMC2302_ADDR, EMC2302_BASE_F123, (0x01 << 0) | (0x01 << 3)));

    // manual fan control
    ESP_ERROR_CHECK(i2c_master_register_write_byte(EMC2302_ADDR, EMC2302_FAN1 + EMC2302_OFS_FAN_CONFIG1, 0x00));
    ESP_ERROR_CHECK(i2c_master_register_write_byte(EMC2302_ADDR, EMC2302_FAN2 + EMC2302_OFS_FAN_CONFIG1, 0x00));

    return true;
}
