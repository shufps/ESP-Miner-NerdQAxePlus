#include "tmp451.h"
#include "i2c_master.h"
#include <esp_check.h>

Tmp451::Tmp451(uint8_t i2c_addr)
    : m_addr(i2c_addr)
{}

esp_err_t Tmp451::init()
{
    uint8_t st = 0;
    if (read_reg(REG_STATUS, &st) != ESP_OK) {
        ESP_LOGE(TAG, "TMP451 not responding at 0x%02X", m_addr);
        return ESP_ERR_NOT_FOUND;
    }

    // Put TMP451 into shutdown (Config1 bit6=1) for one-shot usage
    uint8_t cfg = (1u << 6);
    ESP_RETURN_ON_ERROR(write_reg(REG_CONFIG1_W, cfg), TAG, "config1 write");

    // set ideality to default value
    ESP_RETURN_ON_ERROR(write_reg(REG_NFACTOR_W, 0x00), TAG, "ideality");

    // set offset to zero
    ESP_RETURN_ON_ERROR(write_reg(REG_RTOFFS_MSB, 0x00), TAG, "offset msb");
    ESP_RETURN_ON_ERROR(write_reg(REG_RTOFFS_LSB, 0x00), TAG, "offset lsb");

    return ESP_OK;
}

float Tmp451::get_temperature(int channel)
{
    // -1 = local temperature
    if (channel == -1) {
        return read_local_celsius();
    }

    if (channel < 0 || channel > 3) {
        return NAN;
    }

    if (select_channel(channel) != ESP_OK) {
        return NAN;
    }

    vTaskDelay(pdMS_TO_TICKS(m_wait_after_switch_ms));

    // throw away first conversion
    (void)read_remote_celsius();

    vTaskDelay(pdMS_TO_TICKS(m_wait_before_read_ms));

    float raw = read_remote_celsius();
    m_last_raw[channel] = raw;
    return temp_correct(channel, raw);
}

float Tmp451::get_raw_temperature(int channel)
{
    if (channel < 0 || channel > 3) {
        return NAN;
    }
    return m_last_raw[channel];
}

esp_err_t Tmp451::select_channel(int channel)
{
    (void)channel;
    return ESP_OK; // base class: no mux
}

bool Tmp451::readLocalTemp(float* out_C)
{
    if (!out_C) return false;

    float v = read_local_celsius();
    if (isnan(v)) return false;

    *out_C = v;
    return true;
}

bool Tmp451::readStatus(uint8_t* out_status)
{
    if (!out_status) return false;
    return read_reg(REG_STATUS, out_status) == ESP_OK;
}

float Tmp451::read_remote_celsius()
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

float Tmp451::read_local_celsius()
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

esp_err_t Tmp451::read_reg(uint8_t reg, uint8_t* out)
{
    return i2c_master_register_read(m_addr, reg, out, 1);
}

esp_err_t Tmp451::write_reg(uint8_t reg, uint8_t val)
{
    return i2c_master_register_write_byte(m_addr, reg, val);
}

esp_err_t Tmp451::oneShotAndWait(uint32_t timeout_ms)
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

float Tmp451::temp_correct(int ch, float t_meas)
{
    if (ch < 0 || ch >= 4) {
        return NAN;
    }

    return ((t_meas - 30.0f) * m_cal_scale + 30.0f) + m_cal_off[ch];
}
