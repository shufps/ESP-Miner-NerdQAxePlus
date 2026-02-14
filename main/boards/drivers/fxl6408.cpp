#include "i2c_master.h"
#include "fxl6408.h"
#include <esp_check.h>
#include "esp_timer.h"

#define REG_CTRL          0x01
#define REG_DIRECTION     0x03
#define REG_OUTPUT_STATE  0x05
#define REG_OUTPUT_HZ     0x07
#define REG_INPUT_STATUS  0x0f

static const char* TAG = "fxl6408";

Fxl6408::Fxl6408()
    : m_direction(0x00),      // default all inputs
      m_output_state(0x00)
{
    m_address = 0x43;
}

bool Fxl6408::init()
{
    uint8_t value = 0xff;
    esp_err_t err = read_reg(REG_CTRL, &value);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "no device code");
        return false;
    }
    ESP_LOGI(TAG, "device code: %02x", value);

    // software reset
    err = write_reg(REG_CTRL, 0x01);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "error resetting");
    }

    vTaskDelay(pdMS_TO_TICKS(100));

    // disable output high-z
    err = write_reg(REG_OUTPUT_HZ, 0x00);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "error turning off output HZ");
    }

    vTaskDelay(pdMS_TO_TICKS(100));

    return true;
}

esp_err_t Fxl6408::write_reg(uint8_t reg, uint8_t value)
{
    return i2c_master_register_write_byte(m_address, reg, value);
}

esp_err_t Fxl6408::read_reg(uint8_t reg, uint8_t *out)
{
    return i2c_master_register_read(m_address, reg, out, 1);
}

esp_err_t Fxl6408::set_direction(uint8_t pin, bool output)
{
    if (pin > 7)
        return ESP_ERR_INVALID_ARG;

    if (output)
        m_direction |= (1 << pin);
    else
        m_direction &= ~(1 << pin);

    esp_err_t err = write_reg(REG_DIRECTION, m_direction);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "error writing direction");
    }
    return err;
}

esp_err_t Fxl6408::write(uint8_t pin, bool level)
{
    if (pin > 7)
        return ESP_ERR_INVALID_ARG;

    if (level)
        m_output_state |= (1 << pin);
    else
        m_output_state &= ~(1 << pin);

    esp_err_t err = write_reg(REG_OUTPUT_STATE, m_output_state);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "error writing output");
    }
    return err;
}

esp_err_t Fxl6408::inputStatus(uint8_t* value) {
    *value = 0xaa;
    return read_reg(REG_INPUT_STATUS, value);
}
