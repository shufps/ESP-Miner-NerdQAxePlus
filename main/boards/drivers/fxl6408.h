#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"
#include <stdint.h>

class Fxl6408
{
public:
    Fxl6408();

    bool init();

    esp_err_t set_direction(uint8_t pin, bool output);
    esp_err_t write(uint8_t pin, bool level);
    esp_err_t inputStatus(uint8_t* value);

private:
    esp_err_t write_reg(uint8_t reg, uint8_t value);
    esp_err_t read_reg(uint8_t reg, uint8_t *out);

private:
    uint8_t                  m_address;

    uint8_t m_direction;     // 1 = input, 0 = output
    uint8_t m_output_state;  // output levels
};
