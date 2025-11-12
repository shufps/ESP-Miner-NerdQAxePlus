#include "global_state.h"
#include "hashrate_monitor_task.h"
#include "boards/board.h"
#include "esp_log.h"
#include "mining.h" // for Asic

static const char *HR_TAG = "hashrate_monitor";
static constexpr uint8_t REG_NONCE_TOTAL_CNT = 0x90;

HashrateMonitor::HashrateMonitor()
{}

bool HashrateMonitor::start(Board *board, Asic *asic, uint32_t period_ms, uint32_t window_ms, uint32_t settle_ms)
{
    m_board = board;
    m_asic = asic;
    m_period_ms = period_ms;
    m_window_ms = window_ms;
    m_settle_ms = settle_ms;

    if (!m_board || !m_asic) {
        ESP_LOGE(HR_TAG, "start(): missing dependencies (board=%p, asic=%p)", (void *) m_board, (void *) m_asic);
        return false;
    }

    m_asicCount = board->getAsicCount();

    m_chipHashrate = new float[m_asicCount]();


    xTaskCreate(&HashrateMonitor::taskWrapper, "hr_monitor", 4096, (void *) this, 10, NULL);
    ESP_LOGI(HR_TAG, "started (period=%lums, window=%lums, settle=%lums)", m_period_ms, m_window_ms, m_settle_ms);
    return true;
}

void HashrateMonitor::setChipHashrate(int nr, float temp) {
    if (nr < 0 || nr >= m_asicCount) {
        return;
    }
    m_chipHashrate[nr] = temp;
}

float HashrateMonitor::getChipHashrate(int nr) {
    if (nr < 0 || nr >= m_asicCount) {
        return 0.0f;
    }
    return m_chipHashrate[nr];
}

float HashrateMonitor::getTotalChipHashrate() {
    float total = 0.0f;
    for (int i=0;i < m_asicCount; i++) {
        total += m_chipHashrate[i];
    }
    return total;
}

void HashrateMonitor::taskWrapper(void *pv)
{
    auto *self = static_cast<HashrateMonitor *>(pv);
    self->taskLoop();
}

void HashrateMonitor::publishTotalIfComplete()
{
    size_t offset = 0;

    Board* board = SYSTEM_MODULE.getBoard();

    // Iterate through each ASIC and append its count to the log message
    for (int i = 0; i < board->getAsicCount(); i++) {
        offset += snprintf(m_logBuffer + offset, sizeof(m_logBuffer) - offset, "%.2fGH/s / ", getChipHashrate(i));
    }
    if (offset >= 2) {
        m_logBuffer[offset - 2] = 0; // remove trailing slash
    }

    float hashrate = getTotalChipHashrate();

    History *history = SYSTEM_MODULE.getHistory();
    if (hashrate && history) {
        uint64_t timestamp = esp_timer_get_time() / 1000llu;
        history->pushRate(hashrate, timestamp);
    }

    ESP_LOGI(HR_TAG, "chip hashrates: %s (total: %.3fGH/s)", m_logBuffer, hashrate);
}

void HashrateMonitor::taskLoop()
{
    // Small startup delay
    vTaskDelay(pdMS_TO_TICKS(4000));

    TickType_t lastWake = xTaskGetTickCount();
    while (1) {
        if (POWER_MANAGEMENT_MODULE.isShutdown()) {
            ESP_LOGW(HR_TAG, "suspended");
            vTaskSuspend(NULL);
        }

        if (!m_board || !m_asic) {
            vTaskDelay(pdMS_TO_TICKS(m_period_ms));
            continue;
        }

        // Precise start timestamp (µs) taken right before RESET.
        uint64_t m_t0_us = esp_timer_get_time();

        // Send broadcast RESET for 0x8C.
        m_asic->resetCounter(REG_NONCE_TOTAL_CNT);

        // Measurement window (blocking here
        vTaskDelay(pdMS_TO_TICKS(m_window_ms));

        // Measure actual window length in µs and then READ back all counters
        m_window_us = esp_timer_get_time() - m_t0_us;
        m_asic->readCounter(REG_NONCE_TOTAL_CNT);

        // Give RX some time to deliver replies
        // normally they arrive in 10-20ms
        vTaskDelay(pdMS_TO_TICKS(m_settle_ms));

        // apply a slight smoothing
        if (!m_smoothedHashrate) {
            m_smoothedHashrate = getTotalChipHashrate();
        }

        m_smoothedHashrate = 0.5f * m_smoothedHashrate + 0.5f * getTotalChipHashrate();

        //ESP_LOGI(HR_TAG, "smooth: %.3f unsmooth: %.3f", m_smoothedHashrate, getTotalChipHashrate());

        // In case not all replies arrived, still publish what we have
        publishTotalIfComplete();

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(m_period_ms));
    }
}

void HashrateMonitor::onRegisterReply(uint8_t asic_idx, uint32_t data_ticks)
{
    // no valid window yet?
    if (m_window_us == 0) {
        return;
    }

    // GH/s = cnt * 4096 / (window_s) / 1e9  ==> GH/s = cnt * 4.096 / window_us
    double chip_ghs = (double) data_ticks * 4.096e6 * ERRATA_FACTOR / (double) m_window_us;

    if (m_board) {
        //ESP_LOGI(HR_TAG, "hashrate of chip %u: %.3f GH/s", asic_idx, (float) chip_ghs);
        setChipHashrate(asic_idx, chip_ghs);
    }
}
