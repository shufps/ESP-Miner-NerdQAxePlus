#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include <stdint.h>
#include <math.h>

class Tmp451 {
public:
    explicit Tmp451(uint8_t i2c_addr = 0x4c);

    virtual esp_err_t init();

    // -1 = local sensor
    float get_temperature(int channel);
    float get_raw_temperature(int channel);

    void set_settling_ms(uint32_t after_switch_ms, uint32_t before_read_ms) {
        m_wait_after_switch_ms = after_switch_ms;
        m_wait_before_read_ms  = before_read_ms;
    }

    bool readLocalTemp(float* out_C);
    bool readStatus(uint8_t* out_status);

    void set_temp_cal(float scale, float off0, float off1, float off2, float off3) {
        m_cal_scale = scale;
        m_cal_off[0] = off0; m_cal_off[1] = off1;
        m_cal_off[2] = off2; m_cal_off[3] = off3;
    }

protected:
    virtual esp_err_t select_channel(int channel); // default: no mux
    esp_err_t oneShotAndWait(uint32_t timeout_ms = 200);

    float read_remote_celsius();
    float read_local_celsius();

    esp_err_t read_reg(uint8_t reg, uint8_t* out);
    esp_err_t write_reg(uint8_t reg, uint8_t val);

    static inline float make_temp_c(int8_t msb, uint8_t lsb_nibble) {
        return static_cast<float>(msb) + static_cast<float>(lsb_nibble) * 0.0625f;
    }

private:
    float temp_correct(int ch, float t_meas);

    float m_cal_scale = 1.09f;
    float m_cal_off[4] = {-29.5f, -29.5f, -29.5f, -29.5f};

    static constexpr const char* TAG = "Tmp451";

    static constexpr uint8_t REG_LOCAL_MSB   = 0x00;
    static constexpr uint8_t REG_REMOTE_MSB  = 0x01;
    static constexpr uint8_t REG_STATUS      = 0x02;
    static constexpr uint8_t REG_CONFIG1_W   = 0x09;
    static constexpr uint8_t REG_ONE_SHOT_W  = 0x0F;
    static constexpr uint8_t REG_REMOTE_LSB  = 0x10;
    static constexpr uint8_t REG_RTOFFS_MSB  = 0x11;
    static constexpr uint8_t REG_RTOFFS_LSB  = 0x12;
    static constexpr uint8_t REG_LOCAL_LSB   = 0x15;
    static constexpr uint8_t REG_NFACTOR_W   = 0x23;

    float m_last_raw[4] = {NAN, NAN, NAN, NAN};
    uint8_t  m_addr;
    uint32_t m_wait_after_switch_ms = 50;
    uint32_t m_wait_before_read_ms  = 75;
};
