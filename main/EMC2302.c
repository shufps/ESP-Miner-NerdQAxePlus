#include "esp_log.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "EMC2302.h"

const char *TAG = "emc2301";

esp_err_t EMC2302_set_fan_speed(float percent) {
    int value = (int) (percent * 255.0 + 0.5);
    value = (value > 255) ? 255 : value;

    esp_err_t err;

    ESP_LOGI(TAG, "setting fan speed to %.2f%% (0x%02x)", percent, value);

    err = i2c_master_register_write_byte(EMC2302_ADDR, EMC2302_FAN1 + EMC2302_OFS_FAN_SETTING, (uint8_t) value);
    if (err != ESP_OK) {
        return err;
    }
    err = i2c_master_register_write_byte(EMC2302_ADDR, EMC2302_FAN2 + EMC2302_OFS_FAN_SETTING, (uint8_t) value);
    return err;
}

esp_err_t EMC2302_get_fan_speed(uint16_t *dst) {
    esp_err_t err;
    uint8_t tach_lsb, tach_msb;

    // report only first fan
    err = i2c_master_register_read(EMC2302_ADDR, EMC2302_FAN1 + EMC2302_OFS_TACH_READING_MSB, &tach_msb, 1);
    if (err != ESP_OK) {
        *dst = 0;
        return err;
    }
    err = i2c_master_register_read(EMC2302_ADDR, EMC2302_FAN1 + EMC2302_OFS_TACH_READING_LSB, &tach_lsb, 1);
    if (err != ESP_OK) {
        *dst = 0;
        return err;
    }

    // ESP_LOGI(TAG, "Raw Fan Speed = %02X %02X", tach_msb, tach_lsb);

    int rpm_raw = tach_lsb | (tach_msb << 8);

    const int poles = 2;
    const int n = 5; // number of edges measured (typically five for a two-pole fan)

    // no idea why 8 ... should be wrong but works with the NF-A9x14
    // the tacho is about 87Hz at 100%, noctua says 2 cycles per revolution
    // and the emc2301 gives 11900 as raw value
    // this is the only value that actually makes sense tuning it,
    // so we do it :see-no-evil:
    // rpm = 87Hz * 60 / 2 = 2610
    // rpm = 60 * 32768 * 8 * (5-1) / 2 / 11900 = 2643
    const int m = 8; // the multiplier defined by the RANGE bits

    const int ftach = 32768;

    int rpm = 60 * ftach * m * (n - 1) / poles / rpm_raw;

    ESP_LOGI(TAG, "raw fan speed: %d", rpm_raw);

    if (rpm > 65535) {
        ESP_LOGE(TAG, "fan speed RPM > 16bit: %d", rpm);
        // on invalid result set it to 0 to indicate an error
        rpm = 0;
    } else {
        ESP_LOGI(TAG, "fan speed: %dRPM", rpm);
    }

    *dst = rpm;
    return ESP_OK;
}

bool EMC2302_init(bool invertPolarity) {
    esp_err_t err;
    ESP_LOGI(TAG, "initializing EMC2302");

    // set polarity of ch1 and ch2
    err = i2c_master_register_write_byte(EMC2302_ADDR, EMC2302_POLARITY, (invertPolarity) ? 0x03 : 0x00);
    if (err != ESP_OK) {
        return false;
    }

    // set output type to push pull of ch1 and ch2
    err = i2c_master_register_write_byte(EMC2302_ADDR, EMC2302_OUTPUT_CONFIG, 0x03);
    if (err != ESP_OK) {
        return false;
    }

    // set base frequency of ch1 and ch2 to 19.53kHz
    err = i2c_master_register_write_byte(EMC2302_ADDR, EMC2302_BASE_F123, (0x01 << 0) | (0x01 << 3));
    if (err != ESP_OK) {
        return false;
    }

    // manual fan control
    // bits 4-3: 0b01 = 5 edge samples (2 poles)
    err = i2c_master_register_write_byte(EMC2302_ADDR, EMC2302_FAN1 + EMC2302_OFS_FAN_CONFIG1, (0b01 << 3));
    if (err != ESP_OK) {
        return false;
    }

    err = i2c_master_register_write_byte(EMC2302_ADDR, EMC2302_FAN2 + EMC2302_OFS_FAN_CONFIG1, (0b01 << 3));
    if (err != ESP_OK) {
        return false;
    }

    return true;
}
