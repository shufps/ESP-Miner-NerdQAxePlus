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


private:
    static constexpr const char* TAG = "Tmp451Mux";

    gpio_num_t m_mux_a0;
    gpio_num_t m_mux_a1;
    bool m_mux_active_high;
};
