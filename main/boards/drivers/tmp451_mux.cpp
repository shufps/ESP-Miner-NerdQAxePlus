#include "tmp451_mux.h"
#include "i2c_master.h"
#include <esp_check.h>

Tmp451Mux::Tmp451Mux(gpio_num_t mux_a0, gpio_num_t mux_a1, uint8_t i2c_addr, bool mux_active_high)
    : Tmp451(i2c_addr), m_mux_a0(mux_a0), m_mux_a1(mux_a1), m_mux_active_high(mux_active_high)
{}

esp_err_t Tmp451Mux::init()
{
    // MUX select pins
    gpio_config_t g = {};
    g.mode = GPIO_MODE_OUTPUT;
    g.pin_bit_mask = (1ULL << m_mux_a0) | (1ULL << m_mux_a1);
    g.pull_down_en = GPIO_PULLDOWN_DISABLE;
    g.pull_up_en = GPIO_PULLUP_DISABLE;
    g.intr_type = GPIO_INTR_DISABLE;
    ESP_RETURN_ON_ERROR(gpio_config(&g), TAG, "gpio_config");

    return Tmp451::init();
}

esp_err_t Tmp451Mux::select_channel(int channel)
{
    if (channel < 0 || channel > 3) {
        return ESP_ERR_INVALID_ARG;
    }

    int b0 = (channel & 0x1) ? 1 : 0; // A0 = LSB
    int b1 = (channel & 0x2) ? 1 : 0; // A1 = MSB

    if (!m_mux_active_high) {
        b0 ^= 1;
        b1 ^= 1;
    }

    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_set_level(m_mux_a0, b0));
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_set_level(m_mux_a1, b1));
    return ESP_OK;
}
