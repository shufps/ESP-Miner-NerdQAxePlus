#pragma once
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include <stdint.h>
#include <math.h>

class Tmp451Mux {
public:
    Tmp451Mux(gpio_num_t mux_a0 = GPIO_NUM_2,
              gpio_num_t mux_a1 = GPIO_NUM_3,
              uint8_t i2c_addr = 0x4c,
              bool mux_active_high = true);

    // Init ONLY the MUX GPIOs and put TMP451 into shutdown for one-shot use.
    esp_err_t init();

    // Read remote temperature (°C) on MUX channel 0..3; returns NAN on error.
    float get_temperature(int channel);

    // Select MUX channel 0..3.
    esp_err_t select_channel(int channel);

    // Optional: tune internal waits around a channel switch.
    void set_settling_ms(uint32_t after_switch_ms, uint32_t before_read_ms) {
        m_wait_after_switch_ms = after_switch_ms;
        m_wait_before_read_ms  = before_read_ms;
    }

    // ---- Extra helpers ----
    bool  readLocalTemp(float* out_C);                 // Local temperature (°C)
    bool  readStatus(uint8_t* out_status);             // Read Status (0x02)
    esp_err_t oneShotAndWait(uint32_t timeout_ms = 200);

private:
    // ---- TMP451 register map (READ pointers unless noted) ----
    static constexpr uint8_t REG_LOCAL_MSB   = 0x00;
    static constexpr uint8_t REG_REMOTE_MSB  = 0x01;
    static constexpr uint8_t REG_STATUS      = 0x02; // bit7 = BUSY(1=converting)
    static constexpr uint8_t REG_CONFIG1_W   = 0x09; // write
    static constexpr uint8_t REG_CONVRATE_W  = 0x0A; // write
    static constexpr uint8_t REG_ONE_SHOT_W  = 0x0F; // write any value
    static constexpr uint8_t REG_REMOTE_LSB  = 0x10; // fractional [7:4] = 1/16 °C
    static constexpr uint8_t REG_RTOFFS_MSB  = 0x11; // remote temp offset (signed)
    static constexpr uint8_t REG_RTOFFS_LSB  = 0x12; // offset fractional nibble (assumed)
    static constexpr uint8_t REG_LOCAL_LSB   = 0x15; // fractional [7:4] = 1/16 °C
    static constexpr uint8_t REG_HYST_W      = 0x21; // optional
    static constexpr uint8_t REG_CONAL_W     = 0x22; // optional
    static constexpr uint8_t REG_NFACTOR_W   = 0x23; // η-factor correction
    static constexpr uint8_t REG_DFILTER_W   = 0x24; // DF[1:0]

    static constexpr const char* TAG = "Tmp451Mux";

    // Internals
    float read_remote_celsius();
    float read_local_celsius();
    esp_err_t read_reg(uint8_t reg, uint8_t* out);
    esp_err_t write_reg(uint8_t reg, uint8_t val);

    float temp_correct(int ch, float t_meas);

    static inline float make_temp_c(int8_t msb, uint8_t lsb_nibble) {
        return static_cast<float>(msb) + static_cast<float>(lsb_nibble) * 0.0625f;
    }

private:
    gpio_num_t m_mux_a0;
    gpio_num_t m_mux_a1;
    uint8_t    m_addr;
    bool       m_mux_active_high;

    uint32_t   m_wait_after_switch_ms = 50;
    uint32_t   m_wait_before_read_ms  = 75;
};
