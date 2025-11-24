#include "esp_log.h"
#include <stdio.h>
#include <math.h>
#include "EMC2101.h"

static const char * TAG = "EMC2101";

// run this first. sets up the config register
bool EMC2101_init(bool invertPolarity)
{

    // set the TACH input
    esp_err_t ret = i2c_master_register_write_byte(EMC2101_I2CADDR_DEFAULT, EMC2101_REG_CONFIG, 0x04);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write 0x%02X to EMC2101 register 0x%02X, error: 0x%X", 0x04, EMC2101_REG_CONFIG, ret);
        return false;
    }

    ESP_LOGI(TAG, "Successfully wrote 0x%02X to EMC2101 register 0x%02X", 0x04, EMC2101_REG_CONFIG);

    EMC2101_set_fan_polarity(invertPolarity);
    return true;
}

bool EMC2101_set_fan_polarity(bool invert) {
    esp_err_t ret = i2c_master_register_write_byte(EMC2101_I2CADDR_DEFAULT, EMC2101_FAN_CONFIG, 0b00100011 | (invert ? 0 : (1<<4)));
    return ret == ESP_OK;
}

// takes a fan speed percent
void EMC2101_set_fan_speed(int percent)
{
    int value = (int) roundf((float) percent * 0.63f);

    value = (value > 63) ? 63 : value;
    value = (value < 0) ? 0 : value;

    esp_err_t err = i2c_master_register_write_byte(EMC2101_I2CADDR_DEFAULT, EMC2101_REG_FAN_SETTING, (uint8_t) value);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "error setting fan speed");
    }
}

// RPM = 5400000/reading
uint16_t EMC2101_get_fan_speed(void)
{
    uint8_t tach_lsb, tach_msb;
    uint16_t reading;
    uint16_t RPM;

    esp_err_t err = i2c_master_register_read(EMC2101_I2CADDR_DEFAULT, EMC2101_TACH_LSB, &tach_lsb, 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "error reading tach LSB");
        return 0;
    }

    err = i2c_master_register_read(EMC2101_I2CADDR_DEFAULT, EMC2101_TACH_MSB, &tach_msb, 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "error reading tach MSB");
        return 0;
    }

    // ESP_LOGI(TAG, "Raw Fan Speed = %02X %02X", tach_msb, tach_lsb);

    reading = tach_lsb | (tach_msb << 8);

    if (reading == 0xffff) {
        return 0;
    }

    RPM = 5400000 / reading;

    // ESP_LOGI(TAG, "Fan Speed = %d RPM", RPM);
    return RPM;
}

float EMC2101_get_external_temp(void)
{
    uint8_t temp_msb, temp_lsb;
    uint16_t reading;

    esp_err_t err = i2c_master_register_read(EMC2101_I2CADDR_DEFAULT, EMC2101_EXTERNAL_TEMP_MSB, &temp_msb, 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "error getting external temp MSB");
        return 0.0f;
    }

    err = i2c_master_register_read(EMC2101_I2CADDR_DEFAULT, EMC2101_EXTERNAL_TEMP_LSB, &temp_lsb, 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "error getting external temp LSB");
        return 0.0f;
    }

    // Combine MSB and LSB, and then right shift to get 11 bits
    reading = (temp_msb << 8) | temp_lsb;
    reading >>= 5;  // Now, `reading` contains an 11-bit signed value

    // Cast `reading` to a signed 16-bit integer
    int16_t signed_reading = (int16_t)reading;

    // If the 11th bit (sign bit in 11-bit data) is set, extend the sign
    if (signed_reading & 0x0400) {
        signed_reading |= 0xF800;  // Set upper bits to extend the sign
    }

    if (signed_reading == EMC2101_TEMP_FAULT_OPEN_CIRCUIT) {
        ESP_LOGE(TAG, "EMC2101 TEMP_FAULT_OPEN_CIRCUIT: %04X", signed_reading);
    }
    if (signed_reading == EMC2101_TEMP_FAULT_SHORT) {
        ESP_LOGE(TAG, "EMC2101 TEMP_FAULT_SHORT: %04X", signed_reading);
    }

    // Convert the signed reading to temperature in Celsius
    float result = (float)signed_reading / 8.0;

    return result;
}

uint8_t EMC2101_get_internal_temp(void)
{
    uint8_t temp;
    esp_err_t err = i2c_master_register_read(EMC2101_I2CADDR_DEFAULT, EMC2101_INTERNAL_TEMP, &temp, 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "error reading internal temp");
        return 0;
    }
    return temp;
}

void EMC2101_set_ideality_factor(uint8_t ideality){
    //set Ideality Factor
    esp_err_t err = i2c_master_register_write_byte(EMC2101_I2CADDR_DEFAULT, EMC2101_IDEALITY_FACTOR, ideality);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "error setting ideality factor");
    }
}

void EMC2101_set_beta_compensation(uint8_t beta){
    //set Beta Compensation
    esp_err_t err = i2c_master_register_write_byte(EMC2101_I2CADDR_DEFAULT, EMC2101_BETA_COMPENSATION, beta);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "error setting beta compensation");
    }

}
