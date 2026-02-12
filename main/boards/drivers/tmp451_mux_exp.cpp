#include "tmp451_mux_exp.h"
#include "fxl6408.h"
#include "i2c_master.h"
#include <esp_check.h>

Tmp451MuxExp::Tmp451MuxExp(Fxl6408 *flx, int a0, int a1, uint8_t i2c_addr, bool mux_active_high)
    : Tmp451(i2c_addr), m_flx(flx), m_a0(a0), m_a1(a1)
{}

esp_err_t Tmp451MuxExp::init()
{
    m_flx->set_direction(m_a0, true);
    m_flx->set_direction(m_a1, true);

    return Tmp451::init();
}
esp_err_t Tmp451MuxExp::select_channel(int channel)
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

    m_flx->write(m_a0, b0);
    m_flx->write(m_a1, b1);
    return ESP_OK;
}
