#pragma once

#include "driver/i2c.h"
#include "esp_err.h"

class TPS53647 {
protected:
    uint8_t m_i2cAddr;
    float m_hwMinVoltage;
    float m_initVOutMin;
    float m_initVOutMax;
    uint8_t m_initOnOffConfig;
    uint8_t m_initOtWarnLimit;
    uint8_t m_initOtFaultLimit;
    bool m_initialized;

    esp_err_t read_byte(uint8_t command, uint8_t *data);
    esp_err_t write_byte(uint8_t command, uint8_t data);
    esp_err_t read_word(uint8_t command, uint16_t *result);
    esp_err_t write_word(uint8_t command, uint16_t data);
    esp_err_t write_command(uint8_t command);

    uint8_t volt_to_vid(float volts);
    float vid_to_volt(uint8_t reg_val);

    float slinear11_to_float(uint16_t value);
    uint16_t float_to_slinear11(float x);

    virtual void set_phases(int num_phases);

public:
    TPS53647();

    virtual bool init(int num_phases, int imax, float ifault);

    void clear_faults();

    float get_temperature();
    float get_pin();
    float get_pout();
    float get_vin();
    float get_iin();
    float get_iout();

    float get_vout();
    void set_vout(float volts);
    uint16_t get_vout_vid();

    void power_enable();
    void power_disable();

    void show_voltage_settings();
    virtual void status();

    uint8_t get_status_byte();

};


#define ESP_LOGIE(b, tag, fmt, ...)                                                                                                \
    do {                                                                                                                           \
        if (b) {                                                                                                                   \
            ESP_LOGI(tag, fmt, ##__VA_ARGS__);                                                                                     \
        } else {                                                                                                                   \
            ESP_LOGE(tag, fmt, ##__VA_ARGS__);                                                                                     \
        }                                                                                                                          \
    } while (0)
