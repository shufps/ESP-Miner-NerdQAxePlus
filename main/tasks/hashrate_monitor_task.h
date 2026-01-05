#pragma once
#include <stddef.h>
#include <stdint.h>
#include <pthread.h>

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define HR_INTERVAL 5000

class Board;
class Asic;

template <size_t N> class Median {
    static_assert(N % 2 == 1, "Median requires odd window size");

    float m_buf[N]{};
    size_t m_idx = 0;

  public:
    explicit Median(float init = float{})
    {
        for (size_t i = 0; i < N; ++i)
            m_buf[i] = init;
    }

    float update(float value)
    {
        m_buf[m_idx] = value;
        m_idx = (m_idx + 1) % N;

        // copy to temp
        float tmp[N];
        for (size_t i = 0; i < N; ++i)
            tmp[i] = m_buf[i];

        // insertion sort (small N → fast)
        for (size_t i = 1; i < N; ++i) {
            float key = tmp[i];
            size_t j = i;
            while (j > 0 && tmp[j - 1] > key) {
                tmp[j] = tmp[j - 1];
                --j;
            }
            tmp[j] = key;
        }

        return tmp[N / 2];
    }
};


class HashrateMonitor {
  private:
    // confirmed by long-term averages
    static constexpr double ERRATA_FACTOR = 1.046;
    char m_logBuffer[256] = {0};

    pthread_mutex_t m_mutex = PTHREAD_MUTEX_INITIALIZER;

    // Task + config
    uint32_t m_period_ms = 1000;

    int m_asicCount = 0;
    float *m_chipHashrate = nullptr;
    float m_smoothedHashrate = 0.0f;
    float m_hashrate = 0.0f;

    Median<5> m_median;

    int64_t *m_prevResponse = nullptr;
    uint32_t *m_prevCounter = nullptr;

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
    bool start(Board *board, Asic *asic);

    // Called from RX dispatcher for each register reply.
    // 'counterNow' is the 32-bit counter (host-endian).
    void onRegisterReply(uint8_t asic_idx, uint32_t counterNow);

    float getSmoothedTotalChipHashrate() {
      return m_smoothedHashrate;
    }

    float getHashrate() {
      return m_hashrate;
    }
};
