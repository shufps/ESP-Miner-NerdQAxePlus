#pragma once

#include "BuckConverter.h"
#include "driver/i2c.h"
#include "esp_err.h"

class TPS53647 : public BuckConverter {
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

    void power_enable();
    void power_disable();

public:
    static constexpr uint16_t DEVICE_CODE = 0x01f0;

    TPS53647();

    bool init(int num_phases, int imax, float ifault) override;

    // reads the PMBus device code; returns 0 on error. Used for VR chip
    // detection before the driver is initialized.
    uint16_t get_device_code();

    void clear_faults() override;

    float get_temperature() override;
    float get_pin() override;
    float get_pout() override;
    float get_vin() override;
    float get_iin() override;
    float get_iout() override;

    float get_vout() override;
    bool set_vout(float volts) override;
    uint16_t get_vout_vid();

    void show_voltage_settings();
    void status() override;

    uint8_t get_status_byte() override;
    uint8_t get_status_iout() override;
    uint8_t get_status_vout() override;
    uint8_t get_status_input() override;
    uint8_t get_status_temp() override;

};


#define ESP_LOGIE(b, tag, fmt, ...)                                                                                                \
    do {                                                                                                                           \
        if (b) {                                                                                                                   \
            ESP_LOGI(tag, fmt, ##__VA_ARGS__);                                                                                     \
        } else {                                                                                                                   \
            ESP_LOGE(tag, fmt, ##__VA_ARGS__);                                                                                     \
        }                                                                                                                          \
    } while (0)
