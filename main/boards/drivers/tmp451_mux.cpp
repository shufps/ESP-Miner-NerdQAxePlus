#include "tmp451_mux.h"
#include "i2c_master.h"
#include <esp_check.h>

struct TempCal
{
    float scale;
    float off[4];
};

static TempCal gCal{1.09f, {-29.5f, -29.5f, -29.5f, -29.5f}};

float Tmp451Mux::temp_correct(int ch, float t_meas)
{
    if (ch < 0 || ch >= 4) {
        return NAN;
    }
    return ((t_meas - 30.0f) * gCal.scale + 30.0f) + gCal.off[ch];
}

Tmp451Mux::Tmp451Mux(gpio_num_t mux_a0, gpio_num_t mux_a1, uint8_t i2c_addr, bool mux_active_high)
    : m_mux_a0(mux_a0), m_mux_a1(mux_a1), m_addr(i2c_addr), m_mux_active_high(mux_active_high)
{}

esp_err_t Tmp451Mux::init()
{
    // MUX select pins
    gpio_config_t g = {};
    g.mode = GPIO_MODE_OUTPUT;
    g.pin_bit_mask = (1ULL << m_mux_a0) | (1ULL << m_mux_a1);
    g.pull_down_en = GPIO_PULLDOWN_DISABLE;
    g.pull_up_en = GPIO_PULLUP_DISABLE;
    g.intr_type = GPIO_INTR_DISABLE;
    ESP_RETURN_ON_ERROR(gpio_config(&g), TAG, "gpio_config");

    // check if the tmp451 is present
    uint8_t st = 0;
    if (read_reg(REG_STATUS, &st) != ESP_OK) {
        ESP_LOGE(TAG, "TMP451 not responding at 0x%02X", m_addr);
        return ESP_ERR_NOT_FOUND;
    }

    // Put TMP451 into shutdown (Config1 bit6=1) for one-shot usage
    uint8_t cfg = (1u << 6);
    ESP_RETURN_ON_ERROR(write_reg(REG_CONFIG1_W, cfg), TAG, "config1 write");

    // set ideality to default value 1.008
    ESP_RETURN_ON_ERROR(write_reg(REG_NFACTOR_W, 0x00), TAG, "ideality");

    // set offset to default value zero
    ESP_RETURN_ON_ERROR(write_reg(REG_RTOFFS_MSB, 0x00), TAG, "offset msb");
    ESP_RETURN_ON_ERROR(write_reg(REG_RTOFFS_LSB, 0x00), TAG, "offset lsb");

    // Default: select channel 0
    return select_channel(0);
}

float Tmp451Mux::get_temperature(int channel)
{
    // special case, -1 is inside the TMP451
    if (channel == -1) {
        return read_local_celsius();
    }
    if (channel < 0 || channel > 3) {
        return NAN;
    }
    // switch channel and wait for a bit
    if (select_channel(channel) != ESP_OK) {
        return NAN;
    }
    vTaskDelay(pdMS_TO_TICKS(m_wait_after_switch_ms));

    // start conversion but throw-away the first one
    (void) read_remote_celsius();

    // wait a little and do the real measurement
    // also apply the ideality factor and offset
    vTaskDelay(pdMS_TO_TICKS(m_wait_before_read_ms));
    return temp_correct(channel, read_remote_celsius());
}

esp_err_t Tmp451Mux::select_channel(int channel)
{
    if (channel < 0 || channel > 3) {
        return ESP_ERR_INVALID_ARG;
    }

    int b0 = (channel & 0x1) ? 1 : 0; // A0 = LSB
    int b1 = (channel & 0x2) ? 1 : 0; // A1 = MSB

    if (!m_mux_active_high) {
        b0 ^= 1;
        b1 ^= 1;
    }

    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_set_level(m_mux_a0, b0));
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_set_level(m_mux_a1, b1));
    return ESP_OK;
}

// ---------- Extra helpers ----------
bool Tmp451Mux::readLocalTemp(float *out_C)
{
    if (!out_C)
        return false;
    float v = read_local_celsius();
    if (isnan(v))
        return false;
    *out_C = v;
    return true;
}

bool Tmp451Mux::readStatus(uint8_t *out_status)
{
    if (!out_status)
        return false;
    return read_reg(REG_STATUS, out_status) == ESP_OK;
}

// ---------- Internals ----------
float Tmp451Mux::read_remote_celsius()
{
    if (oneShotAndWait(200) != ESP_OK)
        return NAN;

    uint8_t msb_u = 0, lsb = 0;
    if (read_reg(REG_REMOTE_MSB, &msb_u) != ESP_OK)
        return NAN;
    if (read_reg(REG_REMOTE_LSB, &lsb) != ESP_OK)
        return NAN;

    int8_t msb = static_cast<int8_t>(msb_u);
    uint8_t nib = (lsb >> 4) & 0x0F;
    return make_temp_c(msb, nib);
}

float Tmp451Mux::read_local_celsius()
{
    if (oneShotAndWait(200) != ESP_OK)
        return NAN;

    uint8_t msb_u = 0, lsb = 0;
    if (read_reg(REG_LOCAL_MSB, &msb_u) != ESP_OK)
        return NAN;
    if (read_reg(REG_LOCAL_LSB, &lsb) != ESP_OK)
        return NAN;

    int8_t msb = static_cast<int8_t>(msb_u);
    uint8_t nib = (lsb >> 4) & 0x0F;
    return make_temp_c(msb, nib);
}

esp_err_t Tmp451Mux::read_reg(uint8_t reg, uint8_t *out)
{
    return i2c_master_register_read(m_addr, reg, out, 1);
}

esp_err_t Tmp451Mux::write_reg(uint8_t reg, uint8_t val)
{
    return i2c_master_register_write_byte(m_addr, reg, val);
}

esp_err_t Tmp451Mux::oneShotAndWait(uint32_t timeout_ms)
{
    ESP_RETURN_ON_ERROR(write_reg(REG_ONE_SHOT_W, 0xFF), TAG, "one_shot");

    const TickType_t t0 = xTaskGetTickCount();
    while (true) {
        uint8_t s = 0;
        if (read_reg(REG_STATUS, &s) != ESP_OK)
            return ESP_FAIL;

        bool busy = (s & 0x80) != 0;
        if (!busy)
            return ESP_OK;

        if (pdTICKS_TO_MS(xTaskGetTickCount() - t0) > timeout_ms) {
            ESP_LOGE(TAG, "one-shot timeout, status=%02x", s);
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
