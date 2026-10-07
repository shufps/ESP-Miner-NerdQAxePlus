#pragma once
#include "driver/gpio.h"
#include "tmp451.h"

class Tmp451Mux : public Tmp451 {
public:
    Tmp451Mux(gpio_num_t mux_a0,
              gpio_num_t mux_a1,
              uint8_t i2c_addr = 0x4c,
              bool mux_active_high = true);

    virtual esp_err_t init() override;
    virtual esp_err_t select_channel(int channel) override;

    // Stop driving the A0/A1 GPIOs (e.g. when the W5500 interposer needs those pins):
    // init()/select_channel() then behave like a plain Tmp451 (single I2C channel).
    void disableMux() { m_mux_a0 = GPIO_NUM_NC; m_mux_a1 = GPIO_NUM_NC; }


private:
    static constexpr const char* TAG = "Tmp451Mux";

    gpio_num_t m_mux_a0;
    gpio_num_t m_mux_a1;
    bool m_mux_active_high;
};
