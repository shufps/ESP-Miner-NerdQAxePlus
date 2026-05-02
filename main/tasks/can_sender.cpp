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

void can_send_hello(const uint8_t mac[6])
{
    twai_message_t msg = {};
    msg.identifier       = CAN_ID_HELLO;
    msg.data_length_code = 6;
    memcpy(msg.data, mac, 6);
    twai_transmit(&msg, pdMS_TO_TICKS(50));
    ESP_LOGD(TAG, "TX HELLO MAC=%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void can_send_assign(const uint8_t mac[6], uint8_t can_id)
{
    twai_message_t msg = {};
    msg.identifier       = CAN_ID_ASSIGN;
    msg.data_length_code = 7;
    memcpy(msg.data, mac, 6);
    msg.data[6] = can_id;
    twai_transmit(&msg, pdMS_TO_TICKS(50));
    ESP_LOGI(TAG, "TX ASSIGN MAC=%02X:%02X:%02X:%02X:%02X:%02X → id=%d",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], can_id);
}

void can_send_master_boot(void)
{
    twai_message_t msg = {};
    msg.identifier       = CAN_ID_MASTER_BOOT;
    msg.data_length_code = 0;
    twai_transmit(&msg, pdMS_TO_TICKS(50));
    ESP_LOGI(TAG, "TX MASTER_BOOT");
}

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

    // Append pool_diff so slave can filter nonces below pool threshold
    uint8_t payload[sizeof(BM1368_job) + sizeof(uint32_t)];
    memcpy(payload, &raw, sizeof(BM1368_job));
    memcpy(payload + sizeof(BM1368_job), &job->pool_diff, sizeof(uint32_t));

    uint32_t can_id = CAN_ID_JOB_BASE | (slave_id & 0x7F);
    ESP_LOGD(TAG, "TX JOB slave=%d ntime=%08lX pool_diff=%lu", slave_id, job->ntime, job->pool_diff);
    send_multiframe(can_id, payload, sizeof(payload));
}
