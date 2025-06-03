
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "rom/gpio.h"

#include "TPS53647.h"
#include "boards/nerdqaxeplus.h"
#include "pmbus_commands.h"

#define I2C_MASTER_NUM ((i2c_port_t) 0)

#define WRITE_BIT I2C_MASTER_WRITE
#define READ_BIT I2C_MASTER_READ
#define ACK_CHECK true
#define ACK_VALUE ((i2c_ack_type_t) 0x0)
#define NACK_VALUE ((i2c_ack_type_t) 0x1)
#define MAX_BLOCK_LEN 32

#define SMBUS_DEFAULT_TIMEOUT (1000 / portTICK_PERIOD_MS)

#define TPS53647_EN_PIN GPIO_NUM_10

//#define _DEBUG_LOG_

static const char *TAG = "TPS53647.c";

TPS53647::TPS53647()
{
    m_i2cAddr = 0x71;
    m_hwMinVoltage = 0.25f;
    m_initVOutMin = 1.005f;
    m_initVOutMax = 1.4f;
    m_initOnOffConfig = 0b00010111;
    m_initOtWarnLimit = 95.0f;
    m_initOtFaultLimit = 125.0f;
}

/**
 * @brief SMBus read byte
 */
esp_err_t TPS53647::read_byte(uint8_t command, uint8_t *data)
{
    esp_err_t err = ESP_FAIL;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, m_i2cAddr << 1 | WRITE_BIT, ACK_CHECK);
    i2c_master_write_byte(cmd, command, ACK_CHECK);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, m_i2cAddr << 1 | READ_BIT, ACK_CHECK);
    i2c_master_read_byte(cmd, data, NACK_VALUE);
    i2c_master_stop(cmd);
    err = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, SMBUS_DEFAULT_TIMEOUT);
    i2c_cmd_link_delete(cmd);

    return err;
}

/**
 * @brief SMBus write byte
 */
esp_err_t TPS53647::write_byte(uint8_t command, uint8_t data)
{
    esp_err_t err = ESP_FAIL;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, m_i2cAddr << 1 | WRITE_BIT, ACK_CHECK);
    i2c_master_write_byte(cmd, command, ACK_CHECK);
    i2c_master_write_byte(cmd, data, ACK_CHECK);
    i2c_master_stop(cmd);
    err = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, SMBUS_DEFAULT_TIMEOUT);
    i2c_cmd_link_delete(cmd);

    return err;
}

esp_err_t TPS53647::write_command(uint8_t command)
{
    esp_err_t err = ESP_FAIL;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, m_i2cAddr << 1 | WRITE_BIT, ACK_CHECK);
    i2c_master_write_byte(cmd, command, ACK_CHECK);
    i2c_master_stop(cmd);
    err = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, SMBUS_DEFAULT_TIMEOUT);
    i2c_cmd_link_delete(cmd);

    return err;
}

/**
 * @brief SMBus read word
 */
esp_err_t TPS53647::read_word(uint8_t command, uint16_t *result)
{
    uint8_t data[2];
    esp_err_t err = ESP_FAIL;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, m_i2cAddr << 1 | WRITE_BIT, ACK_CHECK);
    i2c_master_write_byte(cmd, command, ACK_CHECK);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, m_i2cAddr << 1 | READ_BIT, ACK_CHECK);
    i2c_master_read(cmd, &data[0], 1, ACK_VALUE);
    i2c_master_read_byte(cmd, &data[1], NACK_VALUE);
    i2c_master_stop(cmd);
    err = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, SMBUS_DEFAULT_TIMEOUT);
    i2c_cmd_link_delete(cmd);

    *result = (data[1] << 8) + data[0];

    return err;
}

/**
 * @brief SMBus write word
 */
esp_err_t TPS53647::write_word(uint8_t command, uint16_t data)
{
    esp_err_t err = ESP_FAIL;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, m_i2cAddr << 1 | WRITE_BIT, ACK_CHECK);
    i2c_master_write_byte(cmd, command, ACK_CHECK);
    i2c_master_write_byte(cmd, (uint8_t) (data & 0x00FF), ACK_CHECK);
    i2c_master_write_byte(cmd, (uint8_t) ((data & 0xFF00) >> 8), NACK_VALUE);
    i2c_master_stop(cmd);
    err = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, SMBUS_DEFAULT_TIMEOUT);
    i2c_cmd_link_delete(cmd);

    return err;
}

void TPS53647::set_phases(int num_phases)
{
    if (num_phases < 1 || num_phases > 6) {
        ESP_LOGE(TAG, "number of phases out of range: %d", num_phases);
        return;
    }
    ESP_LOGI(TAG, "setting %d phases", num_phases);
    write_byte(PMBUS_MFR_SPECIFIC_20, (uint8_t) (num_phases - 1));
}

uint8_t TPS53647::volt_to_vid(float volts)
{
    if (volts == 0.0f) {
        return 0x00;
    }

    int register_value = (int) ((volts - m_hwMinVoltage) / 0.005f) + 1;

    // Ensure the register value is within the valid range (0x01 to 0xFF)
    if (register_value > 0xFF) {
        printf("Error: Calculated register value out of range.\n");
        ESP_LOGE(TAG, "ERR - vout register value out of range %d", register_value);
        return 0;
    }
    ESP_LOGD(TAG, "volt_to_vid: %.3f -> 0x%04x", volts, register_value);
    return register_value;
}

float TPS53647::vid_to_volt(uint8_t register_value)
{
    if (register_value == 0x00) {
        return 0.0f;
    }

    float volts = (register_value - 1) * 0.005 + m_hwMinVoltage;
    ESP_LOGD(TAG, "vid_to_volt: 0x%04x -> %.3f", register_value, volts);
    return volts;
}

/**
 * @brief Convert an SLINEAR11 value into an int
 */
float TPS53647::slinear11_to_float(uint16_t value)
{
    // 5 bits exponent in two's complement
    int32_t exponent = value >> 11;

    // 11 bits mantissa in two's complement
    int32_t mantissa = value & 0x7ff;

    // extend signs
    exponent |= (exponent & 0x10) ? 0xffffffe0 : 0;
    mantissa |= (mantissa & 0x400) ? 0xfffff800 : 0;

    // calculate result (mantissa * 2^exponent)
    return mantissa * powf(2.0, exponent);
}

/**
 * @brief Convert a float value into an SLINEAR11
 */
uint16_t TPS53647::float_to_slinear11(float x)
{
    if (x <= 0.0f) {
        ESP_LOGI(TAG, "No negative numbers at this time");
        return 0;
    }
    int32_t e = -16;
    int32_t m;
    while (e <= 15) {
        float scale = powf(2.0f, (float) e);
        float temp = x / scale;
        m = (int32_t) roundf(temp);
        if (m >= 0 && m <= 1023) {
            break;
        }
        e++;
    }
    if (e > 15) {
        ESP_LOGI(TAG, "Could not find a solution");
        return 0;
    }
    uint16_t mantissa_bits = (uint16_t) m & 0x7FF;
    uint16_t exponent_bits = (uint16_t) (e & 0x1F);
    uint16_t value = (exponent_bits << 11) | mantissa_bits;
    return value;
}

void TPS53647::status()
{
    uint8_t status_byte = 0xff;
    uint16_t status_word = 0xffff;
    uint8_t status_vout = 0xff;
    uint8_t status_iout = 0xff;
    uint8_t status_input = 0xff;
    uint8_t status_mfr_specific = 0xff;

    read_byte(PMBUS_STATUS_BYTE, &status_byte);
    read_word(PMBUS_STATUS_WORD, &status_word);
    read_byte(PMBUS_STATUS_VOUT, &status_vout);
    read_byte(PMBUS_STATUS_IOUT, &status_iout);
    read_byte(PMBUS_STATUS_INPUT, &status_input);
    read_byte(PMBUS_STATUS_MFR_SPECIFIC, &status_mfr_specific);

    // suppress weird coms error
    status_byte &= ~0x02;
    status_word &= ~0x0002;

    bool isError = (status_byte || status_word || status_vout || status_iout || status_input || status_mfr_specific);

    ESP_LOGIE(!isError, TAG, "TPS536X7_status  bytes: %02x, word: %04x, vout: %02x, iout: %02x, input: %02x, mfr_spec: %02x",
              status_byte, status_word, status_vout, status_iout, status_input, status_mfr_specific);
}

// Set up the TPS53647 regulator and turn it on
bool TPS53647::init(int num_phases, int imax, float ifault)
{
    ESP_LOGI(TAG, "Initializing the core voltage regulator");

    // Establish communication with regulator
    uint16_t device_code = 0x0000;
    read_word(PMBUS_MFR_SPECIFIC_44, &device_code);

    ESP_LOGI(TAG, "Device Code: %04x", device_code);

    if (device_code != 0x01f0) {
        ESP_LOGE(TAG, "ERROR- cannot find TPS53647 buck controller");
        return false;
    }

    ESP_LOGI(TAG, "found TPS53647 controller");

    // clear flags
    write_command(PMBUS_CLEAR_FAULTS);

    // restore all from nvm
    write_command(PMBUS_RESTORE_DEFAULT_ALL);

    // set ON_OFF config, make sure the buck is switched off
    write_byte(PMBUS_ON_OFF_CONFIG, m_initOnOffConfig);

    // Switch frequency, 500kHz
    write_byte(PMBUS_MFR_SPECIFIC_12, 0x20); // default value

    // set maximum current
    write_byte(PMBUS_MFR_SPECIFIC_10, (uint8_t) imax);

    // operation mode
    // VR12 Mode
    // Enable dynamic phase shedding
    // Slew Rate 0.68mV/us
    write_byte(PMBUS_MFR_SPECIFIC_13, 0x89); // default value

    // set up the ON_OFF_CONFIG
    write_byte(PMBUS_ON_OFF_CONFIG, m_initOnOffConfig);

    // Switch frequency, 500kHz
    write_byte(PMBUS_MFR_SPECIFIC_12, 0x20);

    set_phases(num_phases);

    // temperature
    write_word(PMBUS_OT_WARN_LIMIT, float_to_slinear11(m_initOtWarnLimit));
    write_word(PMBUS_OT_FAULT_LIMIT, float_to_slinear11(m_initOtFaultLimit));

    // Iout current
    // set warn and fault to the same value
    write_word(PMBUS_IOUT_OC_WARN_LIMIT, float_to_slinear11(ifault));
    write_word(PMBUS_IOUT_OC_FAULT_LIMIT, float_to_slinear11(ifault));

    m_initialized = true;

    return true;
}

void TPS53647::power_enable()
{
    gpio_pad_select_gpio(TPS53647_EN_PIN);
    gpio_set_direction(TPS53647_EN_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(TPS53647_EN_PIN, 1);
}

void TPS53647::power_disable()
{
    gpio_pad_select_gpio(TPS53647_EN_PIN);
    gpio_set_direction(TPS53647_EN_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(TPS53647_EN_PIN, 0);
}


void TPS53647::clear_faults() {
    write_command(PMBUS_CLEAR_FAULTS);
}


float TPS53647::get_temperature(void)
{
    uint16_t u16_value = 0;
    float temp = 0.0f;

    if (!m_initialized) {
        return 0.0f;
    }

    // Get temperature (SLINEAR11)
    read_word(PMBUS_READ_TEMPERATURE_1, &u16_value);
    ESP_LOGI(TAG, "raw temp: %04x", u16_value);
    temp = slinear11_to_float(u16_value);
#ifdef _DEBUG_LOG_
    ESP_LOGI(TAG, "Got Temp: %2.3f °C", temp);
#endif
    return temp;
}

float TPS53647::get_pin(void)
{
    uint16_t u16_value = 0;
    float pin = 0.0f;

    if (!m_initialized) {
        return 0.0f;
    }

    // Get voltage input (SLINEAR11)
    read_word(PMBUS_READ_PIN, &u16_value);
    pin = slinear11_to_float(u16_value);
#ifdef _DEBUG_LOG_
    ESP_LOGI(TAG, "Got Pin: %2.3f W", pin);
#endif
    return pin;
}

float TPS53647::get_pout(void)
{
    uint16_t u16_value = 0;
    float pout = 0.0f;

    if (!m_initialized) {
        return 0.0f;
    }

    // Get voltage input (SLINEAR11)
    read_word(PMBUS_READ_POUT, &u16_value);
    pout = slinear11_to_float(u16_value);
#ifdef _DEBUG_LOG_
    ESP_LOGI(TAG, "Got Pout: %2.3f W", pout);
#endif
    return pout;
}

float TPS53647::get_vin(void)
{
    uint16_t u16_value = 0;
    float vin = 0.0f;

    if (!m_initialized) {
        return 0.0f;
    }

    // Get voltage input (SLINEAR11)
    read_word(PMBUS_READ_VIN, &u16_value);
    vin = slinear11_to_float(u16_value);
#ifdef _DEBUG_LOG_
    ESP_LOGI(TAG, "Got Vin: %2.3f V", vin);
#endif
    return vin;
}

float TPS53647::get_vout(void)
{
    uint16_t u16_value = 0;
    float vout = 0.0f;

    if (!m_initialized) {
        return 0.0f;
    }

    read_word(PMBUS_MFR_SPECIFIC_04, &u16_value);

    vout = (float) u16_value * powf(2.0f, -9.0f);
#ifdef _DEBUG_LOG_
    ESP_LOGI(TAG, "Got Vout: %2.3f V", vout);
#endif
    return vout;
}

float TPS53647::get_iin(void)
{
    uint16_t u16_value = 0;
    float iin = 0.0f;

    if (!m_initialized) {
        return 0.0f;
    }

    // Get current output (SLINEAR11)
    read_word(PMBUS_READ_IIN, &u16_value);
    iin = slinear11_to_float(u16_value);

#ifdef _DEBUG_LOG_
    ESP_LOGI(TAG, "Got Iin: %2.3f A", iin);
#endif
    return iin;
}

float TPS53647::get_iout(void)
{
    uint16_t u16_value = 0;
    float iout = 0.0f;

    if (!m_initialized) {
        return 0.0f;
    }

    // Get current output (SLINEAR11)
    read_word(PMBUS_READ_IOUT, &u16_value);
    iout = slinear11_to_float(u16_value);

#ifdef _DEBUG_LOG_
    ESP_LOGI(TAG, "Got Iout: %2.3f A", iout);
#endif
    return iout;
}

/**
 * @brief Sets the core voltage
 * this function controls the regulator ontput state
 * send it the desired output in millivolts
 * A value between TPS53647_INIT_VOUT_MIN and TPS53647_INIT_VOUT_MAX
 * send a 0 to turn off the output
 **/
void TPS53647::set_vout(float volts)
{
    if (volts == 0) {
        // turn off output
        // write_byte(PMBUS_OPERATION, OPERATION_OFF);
        power_disable();
        return;
    }

    // make sure we're in range
    if ((volts < m_initVOutMin) || (volts > m_initVOutMax)) {
        ESP_LOGE(TAG, "ERR- Voltage requested (%f V) is out of range", volts);
        return;
    }

    //    write_byte(PMBUS_OPERATION, OPERATION_ON);
    power_enable();

    // set output voltage
    write_word(PMBUS_VOUT_COMMAND, (uint16_t) volt_to_vid(volts));

    // turn on output
    // write_byte(PMBUS_OPERATION, OPERATION_ON);

    ESP_LOGI(TAG, "Vout changed to %1.2f V", volts);
}

void TPS53647::show_voltage_settings(void)
{
    uint16_t u16_value;
    float f_value;

    ESP_LOGI(TAG, "-----------VOLTAGE---------------------");

    // VOUT_MAX
    read_word(PMBUS_VOUT_MAX, &u16_value);
    f_value = vid_to_volt(u16_value);
    ESP_LOGI(TAG, "Vout Max set to: %f V", f_value);

    // VOUT_MARGIN_HIGH
    read_word(PMBUS_VOUT_MARGIN_HIGH, &u16_value);
    f_value = vid_to_volt(u16_value);
    ESP_LOGI(TAG, "Vout Margin HIGH: %f V", f_value);

    // --- VOUT_COMMAND ---
    read_word(PMBUS_VOUT_COMMAND, &u16_value);
    f_value = vid_to_volt(u16_value);
    ESP_LOGI(TAG, "Vout set to: %f V", f_value);

    // VOUT_MARGIN_LOW
    read_word(PMBUS_VOUT_MARGIN_LOW, &u16_value);
    f_value = vid_to_volt(u16_value);
    ESP_LOGI(TAG, "Vout Margin LOW: %f V", f_value);
}

uint16_t TPS53647::get_vout_vid(void)
{
    // 0x97 is 1.00V
    uint16_t vid = 0x97;
    read_word(PMBUS_VOUT_COMMAND, &vid);
    // ESP_LOGI(TAG, "vout_cmd: %02x", vid);
    return vid;
}

uint8_t TPS53647::get_status_byte(void)
{
    uint8_t status_byte = 0xff;
    read_byte(PMBUS_STATUS_BYTE, &status_byte);
    return status_byte;
}
