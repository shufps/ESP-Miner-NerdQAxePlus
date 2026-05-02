#include "can_master_task.h"

#include <string.h>
#include "driver/twai.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "asic.h"
#include "mining.h"
#include "can_sender.h"
#include "global_state.h"
#include "system.h"
#include "boards/board.h"
#include "hashrate_monitor_task.h"
#include "utils.h"

static const char *TAG = "can_master";

// Nonce payload layout (matches can_slave_task.cpp):
//   [0..3]  nonce
//   [4..7]  rolled_version
//   [8]     job_id
#define NONCE_PAYLOAD_LEN 9

// Generic reassembly buffer (sized for the largest payload: telemetry = 44 bytes)
#define RX_BUF_LEN sizeof(can_slave_telemetry_t)

typedef struct {
    uint8_t buf[RX_BUF_LEN];
    size_t  len;
    bool    in_frame;
} slave_rx_t;

// Telemetry table — updated whenever a full telemetry frame arrives from a slave
static can_slave_telemetry_t s_slave_telemetry[CAN_SLAVE_COUNT] = {};

static void handle_nonce(Board *board, uint8_t slave_id, const uint8_t *buf, size_t len)
{
    if (len != NONCE_PAYLOAD_LEN) {
        ESP_LOGW(TAG, "slave %d unexpected nonce len %d", slave_id, len);
        return;
    }

    uint32_t nonce          = 0;
    uint32_t rolled_version = 0;
    uint8_t  job_id         = 0;
    memcpy(&nonce,          buf + 0, 4);
    memcpy(&rolled_version, buf + 4, 4);
    job_id = buf[8];

    ESP_LOGI(TAG, "slave %d nonce=%08lX job_id=%02X", slave_id, nonce, job_id);

    bm_job *job = slaveAsicJobs[slave_id].getClone(job_id);
    if (!job) {
        ESP_LOGW(TAG, "slave %d unknown job_id 0x%02X", slave_id, job_id);
        return;
    }

    rolled_version |= job->version;

    double nonce_diff = test_nonce_value(job, nonce, rolled_version);

    const char *pool_str = job->pool_id ? "Sec" : "Pri";

    ESP_LOGI(TAG, "(%s) slave=%d job=%02X nonce=%08" PRIX32 " diff=%.1f/pool=%lu/asic=%lu",
             pool_str, slave_id, job_id, nonce,
             nonce_diff, job->pool_diff, (uint32_t) board->getAsicMaxDifficulty());

    // TODO: pushShare() for slave nonces — needs asic_nr from slave
    // (slave_id * asics_per_slave + asic_nr), not yet transmitted over CAN

    if (nonce_diff >= job->pool_diff) {
        STRATUM_MANAGER->submitShare(job->pool_id, job->jobid, job->extranonce2,
                                     job->ntime, nonce, rolled_version, job->version);
    }

    STRATUM_MANAGER->checkForBestDiff(job->pool_id, nonce_diff, job->target);
    STRATUM_MANAGER->checkForFoundBlock(job->pool_id, nonce_diff, job->target);

    free_bm_job(job);
}

static void handle_telemetry(uint8_t slave_id, const uint8_t *buf, size_t len)
{
    if (len != sizeof(can_slave_telemetry_t)) {
        ESP_LOGW(TAG, "slave %d unexpected telemetry len %d (expected %d)",
                 slave_id, len, sizeof(can_slave_telemetry_t));
        return;
    }

    memcpy(&s_slave_telemetry[slave_id], buf, sizeof(can_slave_telemetry_t));

    ESP_LOGD(TAG, "slave %d telemetry: hr=%.1f GH/s temp=%.1f°C pwr=%.1f W shutdown=%d",
             slave_id,
             s_slave_telemetry[slave_id].hashRate,
             s_slave_telemetry[slave_id].temp,
             s_slave_telemetry[slave_id].power,
             s_slave_telemetry[slave_id].shutdown);

    // Recompute total external hashrate from all slaves
    float total = 0.0f;
    for (int i = 0; i < CAN_SLAVE_COUNT; i++) {
        total += s_slave_telemetry[i].hashRate;
    }
    HASHRATE_MONITOR.setExternalHashrate(total);
}

void can_master_task(void *pvParameters)
{
    Board *board = SYSTEM_MODULE.getBoard();

    // Separate reassembly slots: nonce[slave] and telemetry[slave]
    static slave_rx_t nonce_rx[CAN_SLAVE_COUNT]    = {};
    static slave_rx_t telem_rx[CAN_SLAVE_COUNT]    = {};

    ESP_LOGI(TAG, "CAN master receiver started");

    for (;;) {
        twai_message_t msg;
        esp_err_t err = twai_receive(&msg, pdMS_TO_TICKS(100));

        if (err == ESP_ERR_TIMEOUT) {
            continue;
        }
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "twai_receive error: %s", esp_err_to_name(err));
            continue;
        }

        if (msg.data_length_code < 1) {
            continue;
        }

        uint32_t base     = msg.identifier & 0xF80;
        uint8_t  slave_id = (uint8_t)(msg.identifier & 0x7F);

        if (slave_id >= CAN_SLAVE_COUNT) {
            continue;
        }

        if (base != CAN_ID_NONCE_BASE && base != CAN_ID_TELEMETRY_BASE) {
            continue;
        }

        slave_rx_t *s    = (base == CAN_ID_NONCE_BASE) ? &nonce_rx[slave_id] : &telem_rx[slave_id];
        size_t      maxlen = (base == CAN_ID_NONCE_BASE) ? NONCE_PAYLOAD_LEN : sizeof(can_slave_telemetry_t);

        uint8_t seq  = msg.data[0];
        uint8_t *data = &msg.data[1];
        size_t   dlen = msg.data_length_code - 1;

        if (seq == 0x00) {
            if (s->in_frame) {
                ESP_LOGW(TAG, "slave %d new seq=0 while in_frame (base=0x%03lX), discarding %d bytes",
                         slave_id, base, s->len);
            }
            s->len      = 0;
            s->in_frame = true;
        } else if (!s->in_frame) {
            continue;
        }

        if (s->len + dlen > maxlen) {
            ESP_LOGW(TAG, "slave %d overflow (base=0x%03lX), discarding", slave_id, base);
            s->in_frame = false;
            continue;
        }
        memcpy(s->buf + s->len, data, dlen);
        s->len += dlen;

        if (seq != CAN_SEQ_LAST) {
            continue;
        }

        s->in_frame = false;

        if (base == CAN_ID_NONCE_BASE) {
            handle_nonce(board, slave_id, s->buf, s->len);
        } else {
            handle_telemetry(slave_id, s->buf, s->len);
        }
    }
}
