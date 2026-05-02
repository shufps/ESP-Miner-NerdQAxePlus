#include "can_sender.h"

#include <string.h>
#include "driver/twai.h"
#include "esp_log.h"
#include "mining.h"
#include "asic.h"

static const char *TAG = "can_sender";

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static void send_multiframe(uint32_t can_id, const uint8_t *data, size_t len)
{
    size_t  offset = 0;
    uint8_t seq    = 0;

    while (offset < len) {
        size_t remaining = len - offset;
        bool   is_last   = (remaining <= 7);
        size_t chunk     = is_last ? remaining : 7;

        twai_message_t frame = {};
        frame.identifier      = can_id;
        frame.data_length_code = (uint8_t)(1 + chunk);
        frame.data[0]          = is_last ? CAN_SEQ_LAST : seq++;
        memcpy(&frame.data[1], data + offset, chunk);

        esp_err_t err = twai_transmit(&frame, pdMS_TO_TICKS(50));
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "TX failed id=0x%03lX seq=%02X: %s",
                     can_id, frame.data[0], esp_err_to_name(err));
        }

        offset += chunk;
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void can_send_telemetry(uint8_t slave_id, const can_slave_telemetry_t *t)
{
    uint32_t can_id = CAN_ID_TELEMETRY_BASE | (slave_id & 0x7F);
    send_multiframe(can_id, (const uint8_t *) t, sizeof(can_slave_telemetry_t));
}

void can_send_raw_job(uint8_t slave_id, uint8_t job_id, const bm_job *job)
{
    BM1368_job raw = {};

    raw.job_id         = job_id;
    raw.num_midstates  = 0x01;
    memcpy(raw.starting_nonce, &job->starting_nonce, 4);
    memcpy(raw.nbits,          &job->target,         4);
    memcpy(raw.ntime,          &job->ntime,           4);
    memcpy(raw.merkle_root,     job->merkle_root_be,  32);
    memcpy(raw.prev_block_hash, job->prev_block_hash_be, 32);
    memcpy(raw.version,        &job->version,         4);

    uint32_t can_id = CAN_ID_JOB_BASE | (slave_id & 0x7F);
    ESP_LOGD(TAG, "TX JOB slave=%d ntime=%08lX", slave_id, job->ntime);
    send_multiframe(can_id, (const uint8_t *)&raw, sizeof(raw));
}
