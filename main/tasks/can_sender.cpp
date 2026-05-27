#include "can_sender.h"

#include <string.h>
#include "driver/twai.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "mining.h"
#include "asic.h"

static const char *TAG = "can_sender";

// Mutex ensuring multiframe packets are sent atomically —
// prevents interleaving when multiple tasks call can_send_* concurrently.
static SemaphoreHandle_t s_tx_mutex = NULL;

static void tx_lock(void)
{
    if (!s_tx_mutex) s_tx_mutex = xSemaphoreCreateMutex();
    xSemaphoreTake(s_tx_mutex, portMAX_DELAY);
}

static void tx_unlock(void)
{
    xSemaphoreGive(s_tx_mutex);
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

// Try to transmit a single CAN frame. On failure, attempt BUS_OFF recovery.
static esp_err_t can_transmit_with_recovery(twai_message_t *frame)
{
    esp_err_t err = twai_transmit(frame, pdMS_TO_TICKS(50));
    if (err == ESP_OK) return ESP_OK;

    twai_status_info_t status;
    if (err == ESP_ERR_INVALID_STATE &&
        twai_get_status_info(&status) == ESP_OK) {

        if (status.state == TWAI_STATE_BUS_OFF) {
            ESP_LOGW(TAG, "BUS_OFF detected (tx_err=%lu), initiating recovery", status.tx_error_counter);
            twai_initiate_recovery();
            for (int i = 0; i < 20; i++) {
                vTaskDelay(pdMS_TO_TICKS(50));
                if (twai_get_status_info(&status) == ESP_OK &&
                    status.state == TWAI_STATE_RUNNING) {
                    ESP_LOGI(TAG, "CAN bus recovered");
                    return ESP_ERR_INVALID_STATE;
                }
            }
            ESP_LOGW(TAG, "CAN bus recovery timed out, restarting TWAI");
        }

        // STOPPED or recovery failed — restart the driver
        if (twai_get_status_info(&status) == ESP_OK &&
            status.state == TWAI_STATE_STOPPED) {
            if (twai_start() == ESP_OK) {
                ESP_LOGI(TAG, "TWAI restarted from STOPPED state");
            } else {
                ESP_LOGE(TAG, "TWAI restart failed");
            }
        }
    } else if (err == ESP_ERR_TIMEOUT) {
        // TX queue full or no ACK — clear stale frames and log
        if (twai_get_status_info(&status) == ESP_OK) {
            ESP_LOGW(TAG, "TX timeout id=0x%03lX (state=%d tx_err=%lu rx_err=%lu q=%lu)",
                     frame->identifier, status.state, status.tx_error_counter,
                     status.rx_error_counter, status.msgs_to_tx);
            if (status.msgs_to_tx > 0) {
                twai_clear_transmit_queue();
            }
        }
    } else {
        ESP_LOGW(TAG, "TX failed id=0x%03lX: %s", frame->identifier, esp_err_to_name(err));
    }

    return err;
}

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

        can_transmit_with_recovery(&frame);

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
    if (can_transmit_with_recovery(&msg) == ESP_OK) {
        ESP_LOGI(TAG, "TX HELLO MAC=%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
}

void can_send_assign(const uint8_t mac[6], uint8_t can_id)
{
    twai_message_t msg = {};
    msg.identifier       = CAN_ID_ASSIGN;
    msg.data_length_code = 7;
    memcpy(msg.data, mac, 6);
    msg.data[6] = can_id;
    if (can_transmit_with_recovery(&msg) == ESP_OK) {
        ESP_LOGI(TAG, "TX ASSIGN MAC=%02X:%02X:%02X:%02X:%02X:%02X → id=%d",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], can_id);
    }
}

void can_send_master_boot(const uint8_t master_mac[6])
{
    twai_message_t msg = {};
    msg.identifier       = CAN_ID_MASTER_BOOT;
    msg.data_length_code = 6;
    memcpy(msg.data, master_mac, 6);
    if (can_transmit_with_recovery(&msg) == ESP_OK) {
        ESP_LOGI(TAG, "TX MASTER_BOOT MAC=%02X:%02X:%02X:%02X:%02X:%02X",
                 master_mac[0], master_mac[1], master_mac[2],
                 master_mac[3], master_mac[4], master_mac[5]);
    }
}

void can_send_telemetry(uint8_t slave_id, const can_slave_telemetry_t *t)
{
    uint32_t can_id = CAN_ID_TELEMETRY_BASE | (slave_id & 0x7F);
    tx_lock();
    send_multiframe(can_id, (const uint8_t *) t, sizeof(can_slave_telemetry_t));
    tx_unlock();
}

void can_send_config(uint8_t slave_id, const can_slave_config_t *c)
{
    uint32_t can_id = CAN_ID_CONFIG_BASE | (slave_id & 0x7F);
    ESP_LOGD(TAG, "TX CONFIG slave=%d model=%s fw=%s", slave_id, c->deviceModel, c->fwVersion);
    tx_lock();
    send_multiframe(can_id, (const uint8_t *) c, sizeof(can_slave_config_t));
    tx_unlock();
}

void can_send_settings_cmd(uint8_t slave_id, const uint8_t *payload, size_t len)
{
    uint32_t can_id = CAN_ID_SETTINGS_BASE | (slave_id & 0x7F);
    ESP_LOGD(TAG, "TX SETTINGS slave=%d cmd=0x%02X", slave_id, len ? payload[0] : 0xFF);
    tx_lock();
    send_multiframe(can_id, payload, len);
    tx_unlock();
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
    tx_lock();
    send_multiframe(can_id, payload, sizeof(payload));
    tx_unlock();
}
