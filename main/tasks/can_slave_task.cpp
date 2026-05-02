#include "can_slave_task.h"

#include <string.h>
#include "driver/twai.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "asic.h"
#include "can_sender.h"
#include "global_state.h"
#include "boards/board.h"
#include "hashrate_monitor_task.h"

static const char *TAG = "can_slave";

// Nonce response layout sent back to master (9 bytes, 2 CAN frames):
//   [0..3]  nonce
//   [4..7]  rolled_version
//   [8]     job_id
#define NONCE_PAYLOAD_LEN 9

static void send_nonce(uint8_t slave_id, const task_result *result)
{
    uint8_t buf[NONCE_PAYLOAD_LEN];
    memcpy(buf + 0, &result->nonce,          4);
    memcpy(buf + 4, &result->rolled_version, 4);
    buf[8] = result->job_id;

    uint32_t can_id = CAN_ID_NONCE_BASE | (slave_id & 0x7F);

    // Frame 0: SEQ=0x00 + 7 bytes
    twai_message_t f0 = {};
    f0.identifier       = can_id;
    f0.data_length_code = 8;
    f0.data[0]          = 0x00;
    memcpy(&f0.data[1], buf, 7);
    twai_transmit(&f0, pdMS_TO_TICKS(50));

    // Frame 1: SEQ=0xFF (last) + 2 bytes
    twai_message_t f1 = {};
    f1.identifier       = can_id;
    f1.data_length_code = 3;
    f1.data[0]          = CAN_SEQ_LAST;
    memcpy(&f1.data[1], buf + 7, 2);
    twai_transmit(&f1, pdMS_TO_TICKS(50));

    ESP_LOGD(TAG, "TX NONCE slave=%d nonce=%08lX job_id=%02X",
             slave_id, result->nonce, result->job_id);
}

void can_slave_task(void *pvParameters)
{
    Board *board = SYSTEM_MODULE.getBoard();
    Asic  *asics = board->getAsics();

    const uint8_t my_id = board->getCanSlaveId();

    // Reassembly buffer — big enough for one BM1368_job (82 bytes)
    uint8_t  buf[128];
    size_t   buf_len   = 0;
    bool     in_frame  = false;

    ESP_LOGI(TAG, "CAN slave task started (slave_id=%d)", my_id);

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

        // Only handle JOB frames addressed to us
        if (msg.identifier != (uint32_t)(CAN_ID_JOB_BASE | my_id)) {
            continue;
        }

        if (msg.data_length_code < 1) {
            continue;
        }

        uint8_t seq   = msg.data[0];
        uint8_t *data = &msg.data[1];
        size_t   dlen = msg.data_length_code - 1;

        if (seq == 0x00) {
            // Start (or restart) of a new job — always reset
            if (in_frame) {
                ESP_LOGW(TAG, "new job seq=0 while in_frame, discarding %d bytes", buf_len);
            }
            buf_len  = 0;
            in_frame = true;
        } else if (!in_frame) {
            // Continuation/last frame without a start — ignore
            continue;
        }

        // Append payload
        if (buf_len + dlen > sizeof(buf)) {
            ESP_LOGW(TAG, "reassembly overflow, discarding");
            in_frame = false;
            continue;
        }
        memcpy(buf + buf_len, data, dlen);
        buf_len += dlen;

        if (seq == CAN_SEQ_LAST) {
            // Full job received
            in_frame = false;

            if (buf_len != sizeof(BM1368_job)) {
                ESP_LOGW(TAG, "unexpected job size %d (expected %d)", buf_len, sizeof(BM1368_job));
                continue;
            }

            BM1368_job *job = (BM1368_job *) buf;
            ESP_LOGI(TAG, "RX JOB job_id=%02X → sendRawJob", job->job_id);

            asics->sendRawJob(job);
        }
    }
}

void can_slave_result_task(void *pvParameters)
{
    Board  *board   = SYSTEM_MODULE.getBoard();
    Asic   *asics   = board->getAsics();
    uint8_t my_id   = board->getCanSlaveId();

    ESP_LOGI(TAG, "CAN slave result task started (slave_id=%d)", my_id);

    for (;;) {
        task_result result;
        if (!asics->processWork(&result)) {
            continue;
        }

        if (result.is_reg_resp) {
            // Handle register responses (temp, hashrate counter)
            switch (result.reg) {
                case 0xb4:
                    if (result.data & 0x80000000) {
                        float ftemp = (float)(result.data & 0x0000ffff) * 0.171342f - 299.5144f;
                        board->setChipTemp(result.asic_nr, ftemp);
                    }
                    break;
                case 0x90:
                    HASHRATE_MONITOR.onRegisterReply(result.asic_nr, result.data);
                    break;
                default:
                    break;
            }
            continue;
        }

        // Actual nonce found
        ESP_LOGI(TAG, "NONCE nonce=%08lX job_id=%02X → TX to master", result.nonce, result.job_id);
        send_nonce(my_id, &result);
    }
}

void can_slave_telemetry_task(void *pvParameters)
{
    uint8_t slave_id = (uint8_t)(uint32_t) pvParameters;
    Board  *board    = SYSTEM_MODULE.getBoard();

    ESP_LOGI(TAG, "telemetry task started (slave_id=%d)", slave_id);

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));

        if (POWER_MANAGEMENT_MODULE.isShutdown()) {
            // still send telemetry so master knows we shut down
        }

        can_slave_telemetry_t t = {};
        t.version           = CAN_TELEMETRY_VERSION;
        t.temp              = POWER_MANAGEMENT_MODULE.getChipTempMax();
        t.vrTemp            = POWER_MANAGEMENT_MODULE.getVRTemp();
        for (int i = 0; i < 4; i++) {
            t.asicTemps[i] = board->getChipTemp(i);
        }
        t.fanRpm            = (uint16_t) POWER_MANAGEMENT_MODULE.getFanRPM(0);
        t.fanRpm2           = (uint16_t) POWER_MANAGEMENT_MODULE.getFanRPM(1);
        t.fanSpeed          = (uint8_t)  POWER_MANAGEMENT_MODULE.getFanPerc(0);
        t.fanSpeed2         = (uint8_t)  POWER_MANAGEMENT_MODULE.getFanPerc(1);
        t.power             = POWER_MANAGEMENT_MODULE.getPower();
        t.current           = (uint16_t) POWER_MANAGEMENT_MODULE.getCurrent();
        t.coreVoltageActual = (uint16_t) (board->getVout() * 1000.0f);
        t.hashRate          = HASHRATE_MONITOR.getHashrate();
        t.shutdown          = POWER_MANAGEMENT_MODULE.isShutdown() ? 1 : 0;

        can_send_telemetry(slave_id, &t);
    }
}
