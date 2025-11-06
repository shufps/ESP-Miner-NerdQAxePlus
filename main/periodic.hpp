#pragma once
#include <stdint.h>
#include "esp_timer.h"

// Simple, drift-free periodic trigger based on esp_timer_get_time().
// Call due()/run_if_due() regularly from one task (not thread-safe by itself).
class Periodic {
public:
    // period_us: interval in microseconds
    // start_immediately: if true, the first call is due right away
    explicit Periodic(uint64_t period_us, bool start_immediately = false)
        : m_period_us(period_us) {
        uint64_t now = esp_timer_get_time();
        m_next_due = start_immediately ? now : now + m_period_us;
    }

    // Returns true exactly at/after each period boundary (drift-free).
    // If caller was late and multiple periods elapsed, it advances m_next_due
    // by whole steps until it is in the future, and returns true once.
    bool due(uint64_t now_us = esp_timer_get_time()) {
        // Signed diff guards against wrap and compares "now >= next_due"
        if ((int64_t)(now_us - m_next_due) >= 0) {
            // Advance by whole periods to avoid long-term drift
            do {
                m_next_due += m_period_us;
            } while ((int64_t)(now_us - m_next_due) >= 0);
            return true;
        }
        return false;
    }

    // Convenience: run callable if due, return true if it ran.
    template <typename F>
    bool run_if_due(F&& fn) {
        uint64_t now = esp_timer_get_time();
        if (due(now)) {
            fn();
            return true;
        }
        return false;
    }

    // Reset the schedule so the next due is now + period
    void reset(uint64_t now_us = esp_timer_get_time()) {
        m_next_due = now_us + m_period_us;
    }

    void set_period_us(uint64_t period_us) {
        m_period_us = period_us;
        reset();
    }

    uint64_t period_us() const { return m_period_us; }

private:
    uint64_t m_period_us;
    uint64_t m_next_due;
};

// Small helpers for readability
static inline constexpr uint64_t sec_to_us(uint64_t s) { return s * 1000000ULL; }
static inline constexpr uint64_t ms_to_us(uint64_t ms) { return ms * 1000ULL; }
