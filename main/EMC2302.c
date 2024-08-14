#include "esp_log.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "EMC2302.h"


void EMC2302_set_fan_speed(float percent) {
    return;
    int value = (int) (percent * 255.0 + 0.5);
    value = (value > 255) ? 255 : value;

    ESP_ERROR_CHECK(i2c_master_register_write_byte(EMC2302_ADDR, EMC2302_FAN1 + EMC2302_OFS_FAN_SETTING, value));
    ESP_ERROR_CHECK(i2c_master_register_write_byte(EMC2302_ADDR, EMC2302_FAN2 + EMC2302_OFS_FAN_SETTING, value));
}

uint16_t EMC2302_get_fan_speed(void) {
    return 4000;
    uint8_t tach_lsb, tach_msb;

    // report only first fan
    ESP_ERROR_CHECK(i2c_master_register_read(EMC2302_ADDR, EMC2302_FAN1 + EMC2302_OFS_TACH_READING_LSB, &tach_lsb, 1));
    ESP_ERROR_CHECK(i2c_master_register_read(EMC2302_ADDR, EMC2302_FAN1 + EMC2302_OFS_TACH_READING_MSB, &tach_msb, 1));

    // ESP_LOGI(TAG, "Raw Fan Speed = %02X %02X", tach_msb, tach_lsb);

    return tach_lsb | (tach_msb << 8);
}

bool EMC2302_init(bool invertPolarity) {
    return true;
    //ESP_ERROR_CHECK(i2c_master_register_write_byte(EMC2302_ADDR, EMC2302_CONFIG, 0x40 /* default */));

    // set polarity of ch1 and ch2
    ESP_ERROR_CHECK(i2c_master_register_write_byte(EMC2302_ADDR, EMC2302_POLARITY, (invertPolarity) ? 0x03 : 0x00));

    // set output type to push pull of ch1 and ch2
    ESP_ERROR_CHECK(i2c_master_register_write_byte(EMC2302_ADDR, EMC2302_OUTPUT_CONFIG, 0x03));

    // set base frequency of ch1 and ch2 to 19.53kHz
    ESP_ERROR_CHECK(i2c_master_register_write_byte(EMC2302_ADDR, EMC2302_BASE_F123, (0x01 << 0) | (0x01 << 3)));
    return true;
}
