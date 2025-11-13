#include "global_state.h"
#include "hashrate_monitor_task.h"
#include "boards/board.h"
#include "esp_log.h"
#include "mining.h" // for Asic

static const char *HR_TAG = "hashrate_monitor";
static constexpr uint8_t REG_NONCE_TOTAL_CNT = 0x90;

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


    xTaskCreate(&HashrateMonitor::taskWrapper, "hr_monitor", 4096, (void *) this, 10, NULL);
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

        // read the counters
        m_asic->readCounter(REG_NONCE_TOTAL_CNT);

        // responses normally take 20-30ms, so this is safe
        vTaskDelay(pdMS_TO_TICKS(500));

        publishTotalIfComplete();

        // apply a slight smoothing
        if (!m_smoothedHashrate) {
            m_smoothedHashrate = getTotalChipHashrate();
        }

        m_smoothedHashrate = 0.5f * m_smoothedHashrate + 0.5f * getTotalChipHashrate();

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

    setChipHashrate(asic_idx, chip_ghs);

    m_prevCounter[asic_idx] = counterNow;
    m_prevResponse[asic_idx] = now;
}
