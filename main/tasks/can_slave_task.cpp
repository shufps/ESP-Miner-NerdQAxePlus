#include "can_slave_task.h"

#include <string.h>
#include "driver/twai.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "asic.h"
#include "can_sender.h"
#include "global_state.h"
#include "boards/board.h"
#include "hashrate_monitor_task.h"
#include "mining.h"
#include "mining_utils.h"
#include "nvs_config.h"
#include "esp_system.h"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"

static const char *TAG = "can_slave";

// Dynamically assigned CAN ID — CAN_SLAVE_ID_UNASSIGNED until negotiation completes
volatile uint8_t g_can_slave_id = CAN_SLAVE_ID_UNASSIGNED;


// Job payload = BM1368_job (82 bytes) + pool_diff (4 bytes)
#define JOB_PAYLOAD_LEN (sizeof(BM1368_job) + sizeof(uint32_t))

// Per-job-id store: BM1368_job + pool_diff for nonce filtering
static BM1368_job  s_jobs[256];
static uint32_t    s_pool_diffs[256];
static bool        s_job_valid[256];

// Recover bm_job LE fields from BM1368_job BE fields and call test_nonce_value().
static double calc_nonce_diff(const BM1368_job *j, uint32_t nonce, uint32_t rolled_version)
{
    bm_job tmp = {};
    uint8_t t[32];

    // BM1368_job.prev_block_hash = prev_block_hash_be = reverse_bytes(_prev_block_hash_binary)
    // bm_job.prev_block_hash = swap_endian_words_bin(_prev_block_hash_binary)
    //   → swap_endian_words_bin(reverse_bytes(BM1368_job.prev_block_hash))
    memcpy(t, j->prev_block_hash, 32);
    reverse_bytes(t, 32);
    swap_endian_words_bin(t, tmp.prev_block_hash, 32);

    // BM1368_job.merkle_root = merkle_root_be = reverse_bytes(swap_endian_words(hex_merkle))
    // bm_job.merkle_root = hex2bin(hex_merkle)
    //   → swap_endian_words_bin(reverse_bytes(BM1368_job.merkle_root))
    memcpy(t, j->merkle_root, 32);
    reverse_bytes(t, 32);
    swap_endian_words_bin(t, tmp.merkle_root, 32);

    memcpy(&tmp.ntime,  j->ntime, 4);
    memcpy(&tmp.target, j->nbits, 4);

    return test_nonce_value(&tmp, nonce, rolled_version);
}

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

static void send_slave_config(uint8_t slave_id)
{
    Board *board = SYSTEM_MODULE.getBoard();
    const esp_app_desc_t *app = esp_app_get_description();

    can_slave_config_t c = {};
    strncpy(c.deviceModel, board->getDeviceModel(), sizeof(c.deviceModel) - 1);
    strncpy(c.fwVersion,   app->version,            sizeof(c.fwVersion)   - 1);
    c.freqMhz        = (uint16_t) Config::getAsicFrequency(board->getDefaultAsicFrequency());
    c.voltageMv       = (uint16_t) Config::getAsicVoltage(board->getDefaultAsicVoltageMillis());
    c.fan0Mode        = (uint8_t)  Config::getFanMode(0);
    c.fan0Speed       = (uint8_t)  Config::getFanManualSpeed(0);
    c.fan0TargetTemp  = (uint8_t)  Config::getFanPidTargetTemp(0, board->getPidSettings(0)->targetTemp);
    c.fan0Overheat    = (uint8_t)  Config::getFanOverheatTemp(0);
    c.fan1Mode        = (uint8_t)  Config::getFanMode(1);
    c.fan1Speed       = (uint8_t)  Config::getFanManualSpeed(1);
    c.fan1TargetTemp  = (uint8_t)  Config::getFanPidTargetTemp(1, board->getPidSettings(1)->targetTemp);
    c.fan1Overheat    = (uint8_t)  Config::getFanOverheatTemp(1);
    c.flipScreen      = Config::isFlipScreenEnabled(false) ? 1 : 0;
    c.autoScreenOff   = Config::isAutoScreenOffEnabled()   ? 1 : 0;

    can_send_config(slave_id, &c);
}

static void handle_settings_cmd(const uint8_t *p, size_t len)
{
    if (len < 1) return;
    uint8_t cmd = p[0];
    bool send_config = false;

    switch (cmd) {
        case CAN_CMD_SET_FREQ:
            if (len >= 3) {
                uint16_t freq; memcpy(&freq, p + 1, 2);
                Config::setAsicFrequency(freq);
                SYSTEM_MODULE.getBoard()->loadSettings();
                ESP_LOGI(TAG, "CMD SET_FREQ %dMHz → applied", freq);
                send_config = true;
            }
            break;
        case CAN_CMD_SET_VOLTAGE:
            if (len >= 3) {
                uint16_t mv; memcpy(&mv, p + 1, 2);
                Config::setAsicVoltage(mv);
                SYSTEM_MODULE.getBoard()->loadSettings();
                ESP_LOGI(TAG, "CMD SET_VOLTAGE %dmV → applied", mv);
                send_config = true;
            }
            break;
        case CAN_CMD_SET_FAN:
            if (len >= 6) {
                uint8_t ch = p[1], mode = p[2], speed = p[3], target = p[4], overheat = p[5];
                Config::setFanMode(ch, mode);
                Config::setFanManualSpeed(ch, speed);
                Config::setFanPidTargetTemp(ch, target);
                Config::setFanOverheatTemp(ch, overheat);
                POWER_MANAGEMENT_MODULE.getFanController().loadSettings();
                ESP_LOGI(TAG, "CMD SET_FAN ch%d mode=%d overheat=%d → applied", ch, mode, overheat);
                send_config = true;
            }
            break;
        case CAN_CMD_SET_DISPLAY:
            if (len >= 3) {
                Config::setFlipScreen(p[1] != 0);
                Config::setAutoScreenOff(p[2] != 0);
                SYSTEM_MODULE.getBoard()->loadSettings();
                ESP_LOGI(TAG, "CMD SET_DISPLAY flip=%d autoOff=%d → applied", p[1], p[2]);
                send_config = true;
            }
            break;
        case CAN_CMD_GET_CONFIG:
            send_config = true;
            break;
        case CAN_CMD_SHUTDOWN:
            ESP_LOGW(TAG, "CMD SHUTDOWN → shutting down");
            POWER_MANAGEMENT_MODULE.shutdown();
            break;
        case CAN_CMD_IDENTIFY:
            ESP_LOGI(TAG, "CMD IDENTIFY");
            // TODO: blink display
            break;
        case CAN_CMD_RESTART:
            ESP_LOGI(TAG, "CMD RESTART → restarting");
            vTaskDelay(pdMS_TO_TICKS(100));
            esp_restart();
            break;
        default:
            ESP_LOGW(TAG, "Unknown settings cmd 0x%02X", cmd);
            break;
    }

    if (send_config && g_can_slave_id != CAN_SLAVE_ID_UNASSIGNED) {
        send_slave_config(g_can_slave_id);
    }
}

void can_slave_task(void *pvParameters)
{
    Board *board = SYSTEM_MODULE.getBoard();
    Asic  *asics = board->getAsics();

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    ESP_LOGI(TAG, "CAN slave MAC=%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    typedef enum { SLAVE_UNASSIGNED, SLAVE_ACTIVE } slave_state_t;
    slave_state_t state = SLAVE_UNASSIGNED;

    TickType_t last_hello = xTaskGetTickCount() - pdMS_TO_TICKS(1000); // send immediately
    TickType_t last_job   = 0;

    uint8_t  buf[128];
    size_t   buf_len  = 0;
    bool     in_frame = false;

    for (;;) {
        TickType_t now = xTaskGetTickCount();

        // Send HELLO periodically while unassigned
        if (state == SLAVE_UNASSIGNED && (now - last_hello) >= pdMS_TO_TICKS(1000)) {
            can_send_hello(mac);
            last_hello = now;
        }

        twai_message_t msg;
        esp_err_t err = twai_receive(&msg, pdMS_TO_TICKS(100));

        if (err == ESP_ERR_TIMEOUT) continue;
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "twai_receive error: %s", esp_err_to_name(err));
            continue;
        }

        // ── Negotiation frames ────────────────────────────────────────────────

        if (msg.identifier == CAN_ID_MASTER_BOOT) {
            ESP_LOGI(TAG, "Master boot detected → re-negotiating");
            state          = SLAVE_UNASSIGNED;
            g_can_slave_id = CAN_SLAVE_ID_UNASSIGNED;
            last_hello     = now - pdMS_TO_TICKS(1000);
            in_frame = false;
            buf_len  = 0;
            continue;
        }

        if (msg.identifier == CAN_ID_ASSIGN && msg.data_length_code == 7) {
            if (memcmp(msg.data, mac, 6) == 0) {
                uint8_t id = msg.data[6];
                ESP_LOGI(TAG, "Assigned CAN ID=%d", id);
                g_can_slave_id = id;
                state    = SLAVE_ACTIVE;
                last_job = now;
                send_slave_config(id);
                // Persist master MAC so we can detect fleet changes on next boot.
                // ASSIGN comes from master, master MAC is not in this frame —
                // we learn it from MASTER_BOOT instead (stored on first boot).
            }
            continue;
        }

        if (state == SLAVE_UNASSIGNED) continue;

        // ── Job timeout → re-negotiate ────────────────────────────────────────

        if (last_job && (now - last_job) >= pdMS_TO_TICKS(10000)) {
            ESP_LOGW(TAG, "No job for 10s → re-negotiating");
            state          = SLAVE_UNASSIGNED;
            g_can_slave_id = CAN_SLAVE_ID_UNASSIGNED;
            last_hello     = now - pdMS_TO_TICKS(1000);
            in_frame = false;
            buf_len  = 0;
            continue;
        }

        // ── Settings commands from master ─────────────────────────────────────

        if (msg.identifier == (uint32_t)(CAN_ID_SETTINGS_BASE | g_can_slave_id)) {
            if (msg.data_length_code >= 2 && msg.data[0] == CAN_SEQ_LAST) {
                handle_settings_cmd(&msg.data[1], msg.data_length_code - 1);
            }
            continue;
        }

        // ── JOB frame reassembly ──────────────────────────────────────────────

        if (msg.identifier != (uint32_t)(CAN_ID_JOB_BASE | g_can_slave_id)) continue;
        if (msg.data_length_code < 1) continue;

        uint8_t  seq  = msg.data[0];
        uint8_t *data = &msg.data[1];
        size_t   dlen = msg.data_length_code - 1;

        if (seq == 0x00) {
            if (in_frame) ESP_LOGW(TAG, "new job seq=0 while in_frame, discarding %d bytes", buf_len);
            buf_len  = 0;
            in_frame = true;
        } else if (!in_frame) {
            continue;
        }

        if (buf_len + dlen > sizeof(buf)) {
            ESP_LOGW(TAG, "reassembly overflow, discarding");
            in_frame = false;
            continue;
        }
        memcpy(buf + buf_len, data, dlen);
        buf_len += dlen;

        if (seq == CAN_SEQ_LAST) {
            in_frame = false;
            last_job = now; // reset timeout on successful job

            if (buf_len != JOB_PAYLOAD_LEN) {
                ESP_LOGW(TAG, "unexpected job size %d (expected %d)", buf_len, JOB_PAYLOAD_LEN);
                continue;
            }

            BM1368_job *job = (BM1368_job *) buf;
            uint32_t pool_diff;
            memcpy(&pool_diff, buf + sizeof(BM1368_job), sizeof(uint32_t));

            s_jobs[job->job_id]       = *job;
            s_pool_diffs[job->job_id] = pool_diff;
            s_job_valid[job->job_id]  = true;

            ESP_LOGI(TAG, "RX JOB job_id=%02X pool_diff=%lu → sendRawJob", job->job_id, pool_diff);
            asics->sendRawJob(job);
        }
    }
}

void can_slave_result_task(void *pvParameters)
{
    Board  *board = SYSTEM_MODULE.getBoard();
    Asic   *asics = board->getAsics();

    ESP_LOGI(TAG, "CAN slave result task started");

    for (;;) {
        if (g_can_slave_id == CAN_SLAVE_ID_UNASSIGNED) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        task_result result;
        if (!asics->processWork(&result)) {
            continue;
        }

        if (result.is_reg_resp) {
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

        if (s_job_valid[result.job_id]) {
            uint32_t version;
            memcpy(&version, s_jobs[result.job_id].version, 4);
            double diff = calc_nonce_diff(&s_jobs[result.job_id], result.nonce, result.rolled_version ^ version);
            uint32_t pool_diff = s_pool_diffs[result.job_id];

            if (diff < pool_diff) {
                ESP_LOGI(TAG, "NONCE job=%02X nonce=%08lX diff=%.1f/%lu → drop",
                         result.job_id, result.nonce, diff, pool_diff);
                continue;
            }
            ESP_LOGI(TAG, "NONCE job=%02X nonce=%08lX diff=%.1f/%lu → TX to master",
                     result.job_id, result.nonce, diff, pool_diff);
        } else {
            ESP_LOGI(TAG, "NONCE nonce=%08lX job_id=%02X (no job stored) → TX to master",
                     result.nonce, result.job_id);
        }
        send_nonce(g_can_slave_id, &result);
    }
}

void can_slave_telemetry_task(void *pvParameters)
{
    Board *board = SYSTEM_MODULE.getBoard();

    ESP_LOGI(TAG, "CAN slave telemetry task started");

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));

        uint8_t slave_id = g_can_slave_id;
        if (slave_id == CAN_SLAVE_ID_UNASSIGNED) continue;

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
        t.boardError        = (uint8_t) SYSTEM_MODULE.getBoardError();
        t.freeHeap          = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);

        can_send_telemetry(slave_id, &t);
    }
}
