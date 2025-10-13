#pragma once
#include <stddef.h>
#include <stdint.h>

extern "C"
{
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
}

class Board;
class Asic;

class HashrateMonitor {
  private:
    // confirmed by long-term averages
    const double ERRATA_FACTOR = 1.046;
    char m_logBuffer[256] = {0};

    // Task + config
    uint32_t m_period_ms = 1000;
    uint32_t m_window_ms = 10000;
    uint32_t m_settle_ms = 300;

    uint64_t m_window_us = 0;

    int m_asicCount = 0;
    float *m_chipHashrate = nullptr;
    float m_smoothedHashrate = 0.0f;


    // Task plumbing
    static void taskWrapper(void *pv);
    void taskLoop();

    // Cycle helpers
    void publishTotalIfComplete();

    // Dependencies
    Board *m_board = nullptr;
    Asic *m_asic = nullptr;

    void setChipHashrate(int nr, float temp);
    float getChipHashrate(int nr);
    float getTotalChipHashrate();

  public:
    HashrateMonitor();

    // Start the background task. period_ms = cadence of measurements,
    // window_ms = measurement window length before READ, settle_ms = RX settle time after READ.
    bool start(Board *board, Asic *asic, uint32_t period_ms, uint32_t window_ms, uint32_t settle_ms = 300);

    // Called from RX dispatcher for each register reply.
    // 'data_ticks' is the 32-bit counter (host-endian).
    void onRegisterReply(uint8_t asic_idx, uint32_t data_ticks);

    float getSmoothedTotalChipHashrate() {
      return m_smoothedHashrate;
    }
};
