#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "rom/gpio.h"

#define TPS53647
#define TPS53647_EN_PIN GPIO_NUM_10

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

#define _DEBUG_LOG_

static const char *TAG = "TPS53647.c";

static bool is_initialized = false;

/**
 * @brief SMBus read byte
 */
static esp_err_t smb_read_byte(uint8_t command, uint8_t *data)
{
    esp_err_t err = ESP_FAIL;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, TPS53647_I2CADDR << 1 | WRITE_BIT, ACK_CHECK);
    i2c_master_write_byte(cmd, command, ACK_CHECK);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, TPS53647_I2CADDR << 1 | READ_BIT, ACK_CHECK);
    i2c_master_read_byte(cmd, data, NACK_VALUE);
    i2c_master_stop(cmd);
    err = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, SMBUS_DEFAULT_TIMEOUT);
    i2c_cmd_link_delete(cmd);

    return err;
}

/**
 * @brief SMBus write byte
 */
static esp_err_t smb_write_byte(uint8_t command, uint8_t data)
{
    esp_err_t err = ESP_FAIL;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, TPS53647_I2CADDR << 1 | WRITE_BIT, ACK_CHECK);
    i2c_master_write_byte(cmd, command, ACK_CHECK);
    i2c_master_write_byte(cmd, data, ACK_CHECK);
    i2c_master_stop(cmd);
    err = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, SMBUS_DEFAULT_TIMEOUT);
    i2c_cmd_link_delete(cmd);

    return err;
}

static esp_err_t smb_write_command(uint8_t command)
{
    esp_err_t err = ESP_FAIL;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, TPS53647_I2CADDR << 1 | WRITE_BIT, ACK_CHECK);
    i2c_master_write_byte(cmd, command, ACK_CHECK);
    i2c_master_stop(cmd);
    err = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, SMBUS_DEFAULT_TIMEOUT);
    i2c_cmd_link_delete(cmd);

    return err;
}

/**
 * @brief SMBus read word
 */
static esp_err_t smb_read_word(uint8_t command, uint16_t *result)
{
    uint8_t data[2];
    esp_err_t err = ESP_FAIL;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, TPS53647_I2CADDR << 1 | WRITE_BIT, ACK_CHECK);
    i2c_master_write_byte(cmd, command, ACK_CHECK);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, TPS53647_I2CADDR << 1 | READ_BIT, ACK_CHECK);
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
static esp_err_t smb_write_word(uint8_t command, uint16_t data)
{
    esp_err_t err = ESP_FAIL;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, TPS53647_I2CADDR << 1 | WRITE_BIT, ACK_CHECK);
    i2c_master_write_byte(cmd, command, ACK_CHECK);
    i2c_master_write_byte(cmd, (uint8_t) (data & 0x00FF), ACK_CHECK);
    i2c_master_write_byte(cmd, (uint8_t) ((data & 0xFF00) >> 8), NACK_VALUE);
    i2c_master_stop(cmd);
    err = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, SMBUS_DEFAULT_TIMEOUT);
    i2c_cmd_link_delete(cmd);

    return err;
}

void set_phases(int num_phases)
{
    if (num_phases < 1 || num_phases > 6) {
        ESP_LOGW(TAG, "number of phases out of range: %d", num_phases);
        return;
    }
    ESP_LOGI(TAG, "setting %d phases", num_phases);
    smb_write_byte(PMBUS_MFR_SPECIFIC_20, (uint8_t) (num_phases - 1));
}

uint8_t volt_to_vid(float volts)
{
    if (volts == 0.0f) {
        return 0x00;
    }

    int register_value = (int) ((volts - TPS53647_HW_MIN_VOLTAGE) / 0.005f) + 1;

    // Ensure the register value is within the valid range (0x01 to 0xFF)
    if (register_value > 0xFF) {
        printf("Error: Calculated register value out of range.\n");
        ESP_LOGI(TAG, "ERR - vout register value out of range %d", register_value);
        return 0;
    }
    ESP_LOGD(TAG, "volt_to_vid: %.3f -> 0x%04x", volts, register_value);
    return register_value;
}

float vid_to_volt(uint8_t register_value)
{
    if (register_value == 0x00) {
        return 0.0f;
    }

    float volts = (register_value - 1) * 0.005 + TPS53647_HW_MIN_VOLTAGE;
    ESP_LOGD(TAG, "vid_to_volt: 0x%04x -> %.3f", register_value, volts);
    return volts;
}

/**
 * @brief Convert an SLINEAR11 value into an int
 */
static int slinear11_2_int(uint16_t value)
{
    int exponent, mantissa;
    float result;

    // First 5 bits is exponent in twos-complement
    // check the first bit of the exponent to see if its negative
    if (value & 0x8000) {
        // exponent is negative
        exponent = -1 * (((~value >> 11) & 0x001F) + 1);
    } else {
        exponent = (value >> 11);
    }
    // last 11 bits is the mantissa in twos-complement
    // check the first bit of the mantissa to see if its negative
    if (value & 0x400) {
        // mantissa is negative
        mantissa = -1 * ((~value & 0x03FF) + 1);
    } else {
        mantissa = (value & 0x03FF);
    }

    // calculate result (mantissa * 2^exponent)
    result = mantissa * powf(2.0, exponent);
    return (int) result;
}

/**
 * @brief Convert an SLINEAR11 value into an int
 */
static float slinear11_2_float(uint16_t value)
{
    int exponent, mantissa;
    float result;

    // First 5 bits is exponent in twos-complement
    // check the first bit of the exponent to see if its negative
    if (value & 0x8000) {
        // exponent is negative
        exponent = -1 * (((~value >> 11) & 0x001F) + 1);
    } else {
        exponent = (value >> 11);
    }
    // last 11 bits is the mantissa in twos-complement
    // check the first bit of the mantissa to see if its negative
    if (value & 0x400) {
        // mantissa is negative
        mantissa = -1 * ((~value & 0x03FF) + 1);
    } else {
        mantissa = (value & 0x03FF);
    }

    // calculate result (mantissa * 2^exponent)
    result = mantissa * powf(2.0, exponent);
    return result;
}

/**
 * @brief Convert an int value into an SLINEAR11
 */
static uint16_t int_2_slinear11(int value)
{
    int mantissa;
    int exponent = 0;
    uint16_t result = 0;
    int i;

    // First see if the exponent is positive or negative
    if (value >= 0) {
        // exponent is positive
        for (i = 0; i <= 15; i++) {
            mantissa = value / powf(2.0, i);
            if (mantissa < 1024) {
                exponent = i;
                break;
            }
        }
        if (i == 16) {
            ESP_LOGI(TAG, "Could not find a solution");
            return 0;
        }
    } else {
        // value is negative
        ESP_LOGI(TAG, "No negative numbers at this time");
        return 0;
    }

    result = ((exponent << 11) & 0xF800) + mantissa;

    return result;
}

/**
 * @brief Convert a float value into an SLINEAR11
 */
static uint16_t float_2_slinear11(float value)
{
    int mantissa;
    int exponent = 0;
    uint16_t result = 0;
    int i;

    // First see if the exponent is positive or negative
    if (value > 0) {
        // exponent is negative
        for (i = 0; i <= 15; i++) {
            mantissa = value * powf(2.0, i);
            if (mantissa >= 1024) {
                exponent = i - 1;
                mantissa = value * powf(2.0, exponent);
                break;
            }
        }
        if (i == 16) {
            ESP_LOGI(TAG, "Could not find a solution");
            return 0;
        }
    } else {
        // value is negative
        ESP_LOGI(TAG, "No negative numbers at this time");
        return 0;
    }

    result = (((~exponent + 1) << 11) & 0xF800) + mantissa;

    return result;
}

void TPS53647_status()
{
    uint8_t status_byte = 0xff;
    uint16_t status_word = 0xffff;
    uint8_t status_vout = 0xff;
    uint8_t status_iout = 0xff;
    uint8_t status_input = 0xff;
    uint8_t status_mfr_specific = 0xff;
    uint16_t vout_cmd = 0xffff;

    smb_read_byte(PMBUS_STATUS_BYTE, &status_byte);
    smb_read_word(PMBUS_STATUS_WORD, &status_word);
    smb_read_byte(PMBUS_STATUS_VOUT, &status_vout);
    smb_read_byte(PMBUS_STATUS_IOUT, &status_iout);
    smb_read_byte(PMBUS_STATUS_INPUT, &status_input);
    smb_read_byte(PMBUS_STATUS_MFR_SPECIFIC, &status_mfr_specific);

    ESP_LOGI(TAG, "bytes: %02x, word: %04x, vout: %02x, iout: %02x, input: %02x, mfr_spec: %02x", status_byte, status_word,
             status_vout, status_iout, status_input, status_mfr_specific);
}

// Set up the TPS53647 regulator and turn it on
int TPS53647_init(int num_phases, int imax, float ifault)
{
    ESP_LOGI(TAG, "Initializing the core voltage regulator");

    // Establish communication with regulator
    uint16_t device_code = 0x0000;
    smb_read_word(PMBUS_MFR_SPECIFIC_44, &device_code);

    ESP_LOGI(TAG, "Device Code: %04x", device_code);

    if (device_code != 0x01f0) {
        ESP_LOGI(TAG, "ERROR- cannot find TPS53647 regulator");
        return -1;
    }

    // restore all from nvm
    smb_write_command(PMBUS_RESTORE_DEFAULT_ALL);

    // set ON_OFF config, make sure the buck is switched off
    smb_write_byte(PMBUS_ON_OFF_CONFIG, TPS53647_INIT_ON_OFF_CONFIG);

    // Switch frequency, 500kHz
    smb_write_byte(PMBUS_MFR_SPECIFIC_12, 0x20); // default value

    // set maximum current
    smb_write_byte(PMBUS_MFR_SPECIFIC_10, (uint8_t) imax);

    // operation mode
    // VR12 Mode
    // Enable dynamic phase shedding
    // Slew Rate 0.68mV/us
    smb_write_byte(PMBUS_MFR_SPECIFIC_13, 0x89); // default value

    set_phases(num_phases);

    // set up the ON_OFF_CONFIG
    smb_write_byte(PMBUS_ON_OFF_CONFIG, TPS53647_INIT_ON_OFF_CONFIG);

    // Switch frequency, 500kHz
    smb_write_byte(PMBUS_MFR_SPECIFIC_12, 0x20);

    set_phases(num_phases);

    // temperature
    smb_write_word(PMBUS_OT_WARN_LIMIT, int_2_slinear11(TPS53647_INIT_OT_WARN_LIMIT));
    smb_write_word(PMBUS_OT_FAULT_LIMIT, int_2_slinear11(TPS53647_INIT_OT_FAULT_LIMIT));

    // iout current
    // set warn and fault to the same value
    smb_write_word(PMBUS_IOUT_OC_WARN_LIMIT, float_2_slinear11(ifault));
    smb_write_word(PMBUS_IOUT_OC_FAULT_LIMIT, float_2_slinear11(ifault));

    is_initialized = true;

    return 0;
}

static void TPS53647_power_enable()
{
    gpio_pad_select_gpio(TPS53647_EN_PIN);
    gpio_set_direction(TPS53647_EN_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(TPS53647_EN_PIN, 1);
}

static void TPS53647_power_disable()
{
    gpio_pad_select_gpio(TPS53647_EN_PIN);
    gpio_set_direction(TPS53647_EN_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(TPS53647_EN_PIN, 0);
}

int TPS53647_get_temperature(void)
{
    uint16_t value = 0;
    int temp = 0;

    if (!is_initialized) {
        return 0;
    }

    smb_read_word(PMBUS_READ_TEMPERATURE_1, &value);
    temp = slinear11_2_int(value);
    return temp;
}

float TPS53647_get_pin(void)
{
    uint16_t u16_value = 0;
    float pin = 0.0f;

    if (!is_initialized) {
        return 0.0f;
    }

    // Get voltage input (SLINEAR11)
    smb_read_word(PMBUS_READ_PIN, &u16_value);
    pin = slinear11_2_float(u16_value);
#ifdef _DEBUG_LOG_
    ESP_LOGI(TAG, "Got Pin: %2.3f W", pin);
#endif
    return pin;
}

float TPS53647_get_pout(void)
{
    uint16_t u16_value = 0;
    float pout = 0.0f;

    if (!is_initialized) {
        return 0.0f;
    }

    // Get voltage input (SLINEAR11)
    smb_read_word(PMBUS_READ_POUT, &u16_value);
    pout = slinear11_2_float(u16_value);
#ifdef _DEBUG_LOG_
    ESP_LOGI(TAG, "Got Pout: %2.3f W", pout);
#endif
    return pout;
}

float TPS53647_get_vin(void)
{
    uint16_t u16_value = 0;
    float vin = 0.0f;

    if (!is_initialized) {
        return 0.0f;
    }

    // Get voltage input (SLINEAR11)
    smb_read_word(PMBUS_READ_VIN, &u16_value);
    vin = slinear11_2_float(u16_value);
#ifdef _DEBUG_LOG_
    ESP_LOGI(TAG, "Got Vin: %2.3f V", vin);
#endif
    return vin;
}

float TPS53647_get_vout(void)
{
    uint16_t u16_value = 0;
    float vout = 0.0f;

    if (!is_initialized) {
        return 0.0f;
    }

    smb_read_word(PMBUS_MFR_SPECIFIC_04, &u16_value);

    vout = (float) u16_value * powf(2.0f, -9.0f);
#ifdef _DEBUG_LOG_
    ESP_LOGI(TAG, "Got Vout: %2.3f V", vout);
#endif
    return vout;
}

float TPS53647_get_iin(void)
{
    uint16_t u16_value = 0;
    float iin = 0.0f;

    if (!is_initialized) {
        return 0.0f;
    }

    // Get current output (SLINEAR11)
    smb_read_word(PMBUS_READ_IIN, &u16_value);
    iin = slinear11_2_float(u16_value);

#ifdef _DEBUG_LOG_
    ESP_LOGI(TAG, "Got Iin: %2.3f A", iin);
#endif
    return iin;
}

float TPS53647_get_iout(void)
{
    uint16_t u16_value = 0;
    float iout = 0.0f;

    if (!is_initialized) {
        return 0.0f;
    }

    // Get current output (SLINEAR11)
    smb_read_word(PMBUS_READ_IOUT, &u16_value);
    iout = slinear11_2_float(u16_value);

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
void TPS53647_set_vout(float volts)
{
    if (volts == 0) {
        // turn off output
        // smb_write_byte(PMBUS_OPERATION, OPERATION_OFF);
        TPS53647_power_disable();
        return;
    }

    // make sure we're in range
    if ((volts < TPS53647_INIT_VOUT_MIN) || (volts > TPS53647_INIT_VOUT_MAX)) {
        ESP_LOGI(TAG, "ERR- Voltage requested (%f V) is out of range", volts);
        return;
    }

    //    smb_write_byte(PMBUS_OPERATION, OPERATION_ON);
    TPS53647_power_enable();

    // set output voltage
    smb_write_word(PMBUS_VOUT_COMMAND, (uint16_t) volt_to_vid(volts));

    // turn on output
    // smb_write_byte(PMBUS_OPERATION, OPERATION_ON);

    ESP_LOGI(TAG, "Vout changed to %1.2f V", volts);
}

void TPS53647_show_voltage_settings(void)
{
    uint16_t u16_value;
    float f_value;

    ESP_LOGI(TAG, "-----------VOLTAGE---------------------");

    // VOUT_MAX
    smb_read_word(PMBUS_VOUT_MAX, &u16_value);
    f_value = vid_to_volt(u16_value);
    ESP_LOGI(TAG, "Vout Max set to: %f V", f_value);

    // VOUT_MARGIN_HIGH
    smb_read_word(PMBUS_VOUT_MARGIN_HIGH, &u16_value);
    f_value = vid_to_volt(u16_value);
    ESP_LOGI(TAG, "Vout Margin HIGH: %f V", f_value);

    // --- VOUT_COMMAND ---
    smb_read_word(PMBUS_VOUT_COMMAND, &u16_value);
    f_value = vid_to_volt(u16_value);
    ESP_LOGI(TAG, "Vout set to: %f V", f_value);

    // VOUT_MARGIN_LOW
    smb_read_word(PMBUS_VOUT_MARGIN_LOW, &u16_value);
    f_value = vid_to_volt(u16_value);
    ESP_LOGI(TAG, "Vout Margin LOW: %f V", f_value);
}

uint16_t TPS53647_get_vout_vid(void) {
    // 0x97 is 1.00V
    uint16_t vid = 0x97;
    smb_read_word(PMBUS_VOUT_COMMAND, &vid);
    return vid;
}