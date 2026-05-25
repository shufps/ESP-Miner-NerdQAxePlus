#pragma once
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "fxl6408.h"
#include "tmp451.h"
#include <math.h>
#include <stdint.h>

class Tmp451MuxExp : public Tmp451 {
  public:
    Tmp451MuxExp(Fxl6408 *flx, int a0, int a1, uint8_t i2c_addr, bool mux_active_high);

    // Select MUX channel 0..3.
    virtual esp_err_t select_channel(int channel);
    virtual esp_err_t init();

  private:
    static constexpr const char* TAG = "Tmp451MuxExp";

    Fxl6408 *m_flx = nullptr;
    int m_a0 = 0;
    int m_a1 = 0;
    bool m_mux_active_high;
};
