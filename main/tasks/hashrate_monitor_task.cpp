#include "global_state.h"
#include "hashrate_monitor_task.h"
#include "boards/board.h"
#include "esp_log.h"
#include "mining.h"
#include "utils.h"

static const char *HR_TAG = "hashrate_monitor";
static constexpr uint8_t REG_NONCE_TOTAL_CNT = 0x90;
static constexpr uint8_t REG_HW_ERROR_CNT    = 0x4C;

HashrateMonitor::HashrateMonitor()
{}

bool HashrateMonitor::start(Board *board, Asic *asic)
{
    m_board = board;
    m_asic = asic;
    m_period_ms = HR_INTERVAL;

    if (!m_board || !m_asic) {
        ESP_LOGE(HR_TAG, "start(): missing dependencies (board=%p, asic=%p)", (void *) m_board, (void *) m_asic);
        return false;
    }

    m_asicCount = board->getAsicCount();

    m_chipHashrate = new float[m_asicCount]();

    m_prevResponse = new int64_t[m_asicCount]();
    m_prevCounter = new uint32_t[m_asicCount]();

    m_chipErrorRate = new float[m_asicCount]();
    m_prevErrorResponse = new int64_t[m_asicCount]();
    m_prevErrorCounter = new uint32_t[m_asicCount]();

    xTaskCreatePSRAM(&HashrateMonitor::taskWrapper, "hr_monitor", 4096, (void *) this, 10, NULL);
    ESP_LOGI(HR_TAG, "started (period=%lums)", m_period_ms);
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

void HashrateMonitor::setChipErrorRate(int nr, float ghs) {
    if (nr < 0 || nr >= m_asicCount) {
        return;
    }
    m_chipErrorRate[nr] = ghs;
}

float HashrateMonitor::getChipErrorRate(int nr) {
    if (nr < 0 || nr >= m_asicCount) {
        return 0.0f;
    }
    return m_chipErrorRate[nr];
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

    // apply slight 3 tap median filter to remove weird outliers
    m_hashrate = m_median.update(getTotalChipHashrate());

    ESP_LOGI(HR_TAG, "chip hashrates: %s (total: %.3fGH/s)", m_logBuffer, m_hashrate);
}

void HashrateMonitor::taskLoop()
{
    // Small startup delay
    vTaskDelay(pdMS_TO_TICKS(4000));

    // Send broadcast RESET for counter register once
    m_asic->resetCounter(REG_NONCE_TOTAL_CNT);

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

        // Read the counters one at a time. Two READ_ALL bursts must NOT overlap:
        // a second READ_ALL sent while the first is still streaming truncates the
        // remaining replies (only chip 0 survives). Give each its own settle window.
        m_asic->readCounter(REG_NONCE_TOTAL_CNT);
        vTaskDelay(pdMS_TO_TICKS(500));

        m_asic->readCounter(REG_HW_ERROR_CNT);
        vTaskDelay(pdMS_TO_TICKS(500));

        publishTotalIfComplete();

        // apply a slight smoothing
        if (!m_smoothedHashrate) {
            m_smoothedHashrate = m_hashrate;
        }

        m_smoothedHashrate = 0.5f * m_smoothedHashrate + 0.5f * m_hashrate;

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(m_period_ms));
    }
}

void HashrateMonitor::onRegisterReply(uint8_t asic_idx, uint32_t counterNow)
{
    if (asic_idx >= m_asicCount) {
        ESP_LOGE(HR_TAG, "respnse for invalid asic %d", (int) asic_idx);
        return;
    }

    int64_t now = esp_timer_get_time();

    // first response
    if (!m_prevResponse[asic_idx]) {
        m_prevResponse[asic_idx] = now;
        m_prevCounter[asic_idx] = counterNow;
        return;
    }

    int64_t timeDelta = now - m_prevResponse[asic_idx];
    uint32_t counterDelta = counterNow - m_prevCounter[asic_idx];

    double chip_ghs = (double) counterDelta * (double) 0x100000000uLL / (double) timeDelta / 1000.0;
//    ESP_LOGE("XXX", "m_prevResponse[%d]=%lld now=%lld m_prevCounter[%d]=%lu counterNow=%lu timeDelta=%llu counterDelta=%lu chip_ghs=%.3f",
//        (int) asic_idx, m_prevResponse[asic_idx], now, (int) asic_idx, m_prevCounter[asic_idx], counterNow, timeDelta, counterDelta, chip_ghs);

    setChipHashrate(asic_idx, chip_ghs);

    m_prevCounter[asic_idx] = counterNow;
    m_prevResponse[asic_idx] = now;
}

void HashrateMonitor::onErrorReply(uint8_t asic_idx, uint32_t counterNow)
{
    if (asic_idx >= m_asicCount) {
        ESP_LOGE(HR_TAG, "error response for invalid asic %d", (int) asic_idx);
        return;
    }

    int64_t now = esp_timer_get_time();

    // first response: establish baseline (the 0x4C error counter is cumulative
    // and not reset, so the first delta is taken from here on)
    if (!m_prevErrorResponse[asic_idx]) {
        m_prevErrorResponse[asic_idx] = now;
        m_prevErrorCounter[asic_idx] = counterNow;
        return;
    }

    int64_t timeDelta = now - m_prevErrorResponse[asic_idx];       // microseconds
    uint32_t counterDelta = counterNow - m_prevErrorCounter[asic_idx]; // wraparound-safe

    // The 0x4C error counter increments in the same 2^32-hashes-per-count unit
    // as the 0x90 nonce counter, so we convert it to a hashrate with the exact
    // same formula used for the valid hashrate. The result is the real "error
    // hashrate" in GH/s, comparable to getChipHashrate(): total = valid + error.
    double err_ghs = (timeDelta > 0)
        ? ((double) counterDelta * (double) 0x100000000uLL / (double) timeDelta / 1000.0)
        : 0.0;
    setChipErrorRate(asic_idx, (float) err_ghs);

    m_prevErrorCounter[asic_idx] = counterNow;
    m_prevErrorResponse[asic_idx] = now;
}
