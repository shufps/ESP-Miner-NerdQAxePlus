#include "can_master_task.h"

#include <stddef.h>
#include <string.h>
#include "driver/twai.h"
#include "esp_log.h"
#include "esp_mac.h"
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
#include "nvs_flash.h"
#include "nvs.h"

static const char *TAG = "can_master";

// ── Slave registry — master is authoritative, persisted in NVS ───────────────
//
// NVS layout (namespace "can_master"):
//   key "reg" → blob of CAN_SLAVE_MAX * 7 bytes: [mac[6] | used(1)] per slot
//   Slot 0 is reserved for master (never written).

#define NVS_NAMESPACE  "can_master"
#define NVS_KEY_REG    "reg"
#define NVS_SLOT_BYTES 7   // mac[6] + used[1]
#define NVS_REG_SIZE   (CAN_SLAVE_MAX * NVS_SLOT_BYTES)

typedef struct {
    uint8_t mac[6];
    bool    used;
    bool    foreign;  // not persisted — detected at runtime from HELLO
} slave_reg_entry_t;

static slave_reg_entry_t s_slave_reg[CAN_SLAVE_MAX] = {};
static uint8_t           s_master_mac[6]             = {};

// Forward declarations
static can_slave_telemetry_t s_slave_telemetry[CAN_SLAVE_MAX];
static can_slave_config_t    s_slave_config[CAN_SLAVE_MAX];
static void nvs_save_registry(void);

bool can_master_is_slave_active(uint8_t slave_id)
{
    if (slave_id >= CAN_SLAVE_MAX) return false;
    return s_slave_reg[slave_id].used;
}

bool can_master_is_slave_known(uint8_t slave_id)
{
    if (slave_id >= CAN_SLAVE_MAX) return false;
    return s_slave_reg[slave_id].used;
}

bool can_master_is_slave_foreign(uint8_t slave_id)
{
    if (slave_id >= CAN_SLAVE_MAX) return false;
    return s_slave_reg[slave_id].foreign;
}

bool can_master_get_slave_mac(uint8_t slave_id, uint8_t out[6])
{
    if (slave_id >= CAN_SLAVE_MAX || !s_slave_reg[slave_id].used) return false;
    memcpy(out, s_slave_reg[slave_id].mac, 6);
    return true;
}

bool can_master_get_slave_telemetry(uint8_t slave_id, can_slave_telemetry_t *out)
{
    if (slave_id >= CAN_SLAVE_MAX || !s_slave_reg[slave_id].used) return false;
    memcpy(out, &s_slave_telemetry[slave_id], sizeof(can_slave_telemetry_t));
    return true;
}

bool can_master_get_slave_config(uint8_t slave_id, can_slave_config_t *out)
{
    if (slave_id >= CAN_SLAVE_MAX || !s_slave_reg[slave_id].used) return false;
    memcpy(out, &s_slave_config[slave_id], sizeof(can_slave_config_t));
    return true;
}

bool can_master_get_fleet_shutdown(uint8_t *out_id)
{
    for (int i = 1; i < CAN_SLAVE_MAX; i++) {
        if (s_slave_reg[i].used && s_slave_telemetry[i].shutdown) {
            if (out_id) *out_id = (uint8_t) i;
            return true;
        }
    }
    return false;
}

bool can_master_get_fleet_error(uint8_t *out_id, uint8_t *out_board_error)
{
    for (int i = 1; i < CAN_SLAVE_MAX; i++) {
        if (!s_slave_reg[i].used) continue;
        uint8_t berr = s_slave_telemetry[i].boardError;
        bool shut    = s_slave_telemetry[i].shutdown;
        if (berr != 0 || shut) {
            if (out_id)          *out_id          = (uint8_t) i;
            if (out_board_error) *out_board_error = berr;
            return true;
        }
    }
    return false;
}

float can_master_get_slave_fleet_power(void)
{
    float total = 0.0f;
    for (int i = 1; i < CAN_SLAVE_MAX; i++) {
        if (s_slave_reg[i].used) total += s_slave_telemetry[i].power;
    }
    return total;
}

void can_master_set_slave_freq(uint8_t slave_id, uint16_t freq_mhz)
{
    uint8_t p[3] = { CAN_CMD_SET_FREQ, (uint8_t)(freq_mhz & 0xFF), (uint8_t)(freq_mhz >> 8) };
    can_send_settings_cmd(slave_id, p, sizeof(p));
}

void can_master_set_slave_voltage(uint8_t slave_id, uint16_t mv)
{
    uint8_t p[3] = { CAN_CMD_SET_VOLTAGE, (uint8_t)(mv & 0xFF), (uint8_t)(mv >> 8) };
    can_send_settings_cmd(slave_id, p, sizeof(p));
}

void can_master_set_slave_fan(uint8_t slave_id, uint8_t ch, uint8_t mode,
                              uint8_t speed, uint8_t target_temp, uint8_t overheat)
{
    uint8_t p[6] = { CAN_CMD_SET_FAN, ch, mode, speed, target_temp, overheat };
    can_send_settings_cmd(slave_id, p, sizeof(p));
}

void can_master_set_slave_display(uint8_t slave_id, uint8_t flip, uint8_t auto_off)
{
    uint8_t p[3] = { CAN_CMD_SET_DISPLAY, flip, auto_off };
    can_send_settings_cmd(slave_id, p, sizeof(p));
}

void can_master_shutdown_slave(uint8_t slave_id)
{
    uint8_t p[1] = { CAN_CMD_SHUTDOWN };
    can_send_settings_cmd(slave_id, p, sizeof(p));
}

void can_master_identify_slave(uint8_t slave_id)
{
    uint8_t p[1] = { CAN_CMD_IDENTIFY };
    can_send_settings_cmd(slave_id, p, sizeof(p));
}

void can_master_restart_slave(uint8_t slave_id)
{
    uint8_t p[1] = { CAN_CMD_RESTART };
    can_send_settings_cmd(slave_id, p, sizeof(p));
}

void can_master_delete_slave(uint8_t slave_id)
{
    if (slave_id >= CAN_SLAVE_MAX) return;
    memset(&s_slave_reg[slave_id], 0, sizeof(slave_reg_entry_t));
    memset(&s_slave_telemetry[slave_id], 0, sizeof(can_slave_telemetry_t));
    memset(&s_slave_config[slave_id], 0, sizeof(can_slave_config_t));
    nvs_save_registry();
    ESP_LOGI(TAG, "Deleted slave id=%d from registry", slave_id);
}

static void nvs_load_registry(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) return;

    uint8_t blob[NVS_REG_SIZE] = {};
    size_t  len = sizeof(blob);
    if (nvs_get_blob(h, NVS_KEY_REG, blob, &len) == ESP_OK && len == sizeof(blob)) {
        for (int i = 1; i < CAN_SLAVE_MAX; i++) {
            const uint8_t *slot = blob + i * NVS_SLOT_BYTES;
            s_slave_reg[i].used = slot[6];
            if (s_slave_reg[i].used) {
                memcpy(s_slave_reg[i].mac, slot, 6);
                ESP_LOGI(TAG, "NVS restored id=%d MAC=%02X:%02X:%02X:%02X:%02X:%02X",
                         i, slot[0], slot[1], slot[2], slot[3], slot[4], slot[5]);
            }
        }
    }
    nvs_close(h);
}

static void nvs_save_registry(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) {
        ESP_LOGW(TAG, "NVS open failed, registry not saved");
        return;
    }
    uint8_t blob[NVS_REG_SIZE] = {};
    for (int i = 1; i < CAN_SLAVE_MAX; i++) {
        uint8_t *slot = blob + i * NVS_SLOT_BYTES;
        memcpy(slot, s_slave_reg[i].mac, 6);
        slot[6] = s_slave_reg[i].used ? 1 : 0;
    }
    nvs_set_blob(h, NVS_KEY_REG, blob, sizeof(blob));
    nvs_commit(h);
    nvs_close(h);
}

static void handle_hello(const uint8_t mac[6])
{
    // Known slave (in NVS/RAM) → re-assign same ID
    for (int i = 1; i < CAN_SLAVE_MAX; i++) {
        if (s_slave_reg[i].used && memcmp(s_slave_reg[i].mac, mac, 6) == 0) {
            ESP_LOGI(TAG, "HELLO known slave MAC=%02X:%02X:%02X:%02X:%02X:%02X → id=%d",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], i);
            can_send_assign(mac, (uint8_t) i);
            // Slave sends config automatically after ASSIGN, but request explicitly
            // in case this is a re-negotiation and slave already skipped the send.
            {
                uint8_t p[1] = { CAN_CMD_GET_CONFIG };
                can_send_settings_cmd((uint8_t) i, p, sizeof(p));
            }
            return;
        }
    }
    // New slave → assign next free slot and persist
    for (int i = 1; i < CAN_SLAVE_MAX; i++) {
        if (!s_slave_reg[i].used) {
            memcpy(s_slave_reg[i].mac, mac, 6);
            s_slave_reg[i].used    = true;
            s_slave_reg[i].foreign = false;
            nvs_save_registry();
            ESP_LOGI(TAG, "HELLO new slave MAC=%02X:%02X:%02X:%02X:%02X:%02X → id=%d",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], i);
            can_send_assign(mac, (uint8_t) i);
            return;
        }
    }
    ESP_LOGW(TAG, "HELLO from slave but registry full (max %d)", CAN_SLAVE_MAX);
}

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

static void handle_config(uint8_t slave_id, const uint8_t *buf, size_t len)
{
    if (len < sizeof(can_slave_config_t)) {
        ESP_LOGW(TAG, "slave %d config too short %d (expected %d)",
                 slave_id, len, sizeof(can_slave_config_t));
        return;
    }
    memcpy(&s_slave_config[slave_id], buf, sizeof(can_slave_config_t));
    const can_slave_config_t *c = &s_slave_config[slave_id];
    // Ensure null-termination regardless of sender
    char model[sizeof(c->deviceModel) + 1] = {};
    char fw[sizeof(c->fwVersion)     + 1] = {};
    memcpy(model, c->deviceModel, sizeof(c->deviceModel));
    memcpy(fw,    c->fwVersion,   sizeof(c->fwVersion));
    ESP_LOGI(TAG, "slave %d config: model=%s fw=%s freq=%uMHz volt=%umV", slave_id, model, fw, c->freqMhz, c->voltageMv);
}

static void handle_telemetry(uint8_t slave_id, const uint8_t *buf, size_t len)
{
    if (len < sizeof(can_slave_telemetry_t)) {
        ESP_LOGW(TAG, "slave %d telemetry too short %d (expected %d)",
                 slave_id, len, sizeof(can_slave_telemetry_t));
        return;
    }

    memset(&s_slave_telemetry[slave_id], 0, sizeof(can_slave_telemetry_t));
    memcpy(&s_slave_telemetry[slave_id], buf, len < sizeof(can_slave_telemetry_t) ? len : sizeof(can_slave_telemetry_t));

    const can_slave_telemetry_t *t = &s_slave_telemetry[slave_id];
    ESP_LOGI(TAG, "slave %d | hr=%.1fGH/s | temp=%.1f°C vr=%.1f°C"
             " | fan0=%uRPM(%u%%) fan1=%uRPM(%u%%)"
             " | pwr=%.1fW cur=%umA vout=%umV"
             " | %s",
             slave_id,
             t->hashRate,
             t->temp, t->vrTemp,
             t->fanRpm, t->fanSpeed,
             t->fanRpm2, t->fanSpeed2,
             t->power, t->current, t->coreVoltageActual,
             t->shutdown ? "SHUTDOWN" : "ok");

    // Recompute total external hashrate from all slaves
    float total = 0.0f;
    for (int i = 0; i < CAN_SLAVE_MAX; i++) {
        if (s_slave_reg[i].used) total += s_slave_telemetry[i].hashRate;
    }
    HASHRATE_MONITOR.setExternalHashrate(total);
}

void can_master_task(void *pvParameters)
{
    Board *board = SYSTEM_MODULE.getBoard();

    esp_read_mac(s_master_mac, ESP_MAC_WIFI_STA);
    ESP_LOGI(TAG, "CAN master MAC=%02X:%02X:%02X:%02X:%02X:%02X",
             s_master_mac[0], s_master_mac[1], s_master_mac[2],
             s_master_mac[3], s_master_mac[4], s_master_mac[5]);

    nvs_load_registry();

    // Separate reassembly slots per slave and frame type
    static slave_rx_t nonce_rx[CAN_SLAVE_MAX]  = {};
    static slave_rx_t telem_rx[CAN_SLAVE_MAX]  = {};
    static slave_rx_t config_rx[CAN_SLAVE_MAX] = {};

    ESP_LOGI(TAG, "CAN master receiver started");

    // Announce boot so any already-running slaves re-negotiate immediately
    can_send_master_boot(s_master_mac);

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

        // ── Negotiation frames ────────────────────────────────────────────────
        if (msg.identifier == CAN_ID_HELLO && msg.data_length_code == 6) {
            handle_hello(msg.data);
            continue;
        }

        if (msg.data_length_code < 1) {
            continue;
        }

        uint32_t base     = msg.identifier & 0xF80;
        uint8_t  slave_id = (uint8_t)(msg.identifier & 0x7F);

        if (slave_id >= CAN_SLAVE_MAX) {
            continue;
        }

        if (base != CAN_ID_NONCE_BASE && base != CAN_ID_TELEMETRY_BASE && base != CAN_ID_CONFIG_BASE) {
            continue;
        }

        slave_rx_t *s;
        size_t      maxlen;
        if (base == CAN_ID_NONCE_BASE) {
            s = &nonce_rx[slave_id];  maxlen = NONCE_PAYLOAD_LEN;
        } else if (base == CAN_ID_CONFIG_BASE) {
            s = &config_rx[slave_id]; maxlen = sizeof(can_slave_config_t);
        } else {
            s = &telem_rx[slave_id];  maxlen = sizeof(can_slave_telemetry_t);
        }

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
        } else if (base == CAN_ID_CONFIG_BASE) {
            handle_config(slave_id, s->buf, s->len);
        } else {
            handle_telemetry(slave_id, s->buf, s->len);
        }
    }
}
