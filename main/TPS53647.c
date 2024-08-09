#include "driver/i2c.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <string.h>

#define TPS53647

#include "pmbus_commands.h"
#include "TPS53647.h"

#define I2C_MASTER_NUM 0 /*!< I2C master i2c port number, the number of i2c peripheral interfaces available will depend on the chip */

#define WRITE_BIT      I2C_MASTER_WRITE
#define READ_BIT       I2C_MASTER_READ
#define ACK_CHECK      true
#define NO_ACK_CHECK   false
#define ACK_VALUE      0x0
#define NACK_VALUE     0x1
#define MAX_BLOCK_LEN  32

#define SMBUS_DEFAULT_TIMEOUT (1000 / portTICK_PERIOD_MS)

static const char *TAG = "TPS53647.c";

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
    i2c_set_timeout(I2C_MASTER_NUM, 20);
    ESP_ERROR_CHECK(i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, SMBUS_DEFAULT_TIMEOUT));
    i2c_cmd_link_delete(cmd);

    // return get an actual error status
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
    ESP_ERROR_CHECK(i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, SMBUS_DEFAULT_TIMEOUT));
    i2c_cmd_link_delete(cmd);

    // TODO return an actual error status
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
    i2c_set_timeout(I2C_MASTER_NUM, 20);
    ESP_ERROR_CHECK(i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, SMBUS_DEFAULT_TIMEOUT));
    i2c_cmd_link_delete(cmd);

    *result = (data[1] << 8) + data[0];
    // TODO return an actual error status
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
    i2c_master_write_byte(cmd, (uint8_t)(data & 0x00FF), ACK_CHECK);
    i2c_master_write_byte(cmd, (uint8_t)((data & 0xFF00) >> 8), NACK_VALUE);
    i2c_master_stop(cmd);
    ESP_ERROR_CHECK(i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, SMBUS_DEFAULT_TIMEOUT));
    i2c_cmd_link_delete(cmd);

    // TODO return an actual error status
    return err;
}

/**
 * @brief SMBus read block
 */
static esp_err_t smb_read_block(uint8_t command, uint8_t *data, uint8_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, TPS53647_I2CADDR << 1 | WRITE_BIT, ACK_CHECK);
    i2c_master_write_byte(cmd, command, ACK_CHECK);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, TPS53647_I2CADDR << 1 | READ_BIT, ACK_CHECK);
    uint8_t slave_len = 0;
    i2c_master_read_byte(cmd, &slave_len, ACK_VALUE);
    ESP_ERROR_CHECK(i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, SMBUS_DEFAULT_TIMEOUT));
    i2c_cmd_link_delete(cmd);

    cmd = i2c_cmd_link_create();
    for (size_t i = 0; i < slave_len - 1; ++i)
    {
        i2c_master_read_byte(cmd, &data[i], ACK_VALUE);
    }
    i2c_master_read_byte(cmd, &data[slave_len - 1], NACK_VALUE);
    i2c_master_stop(cmd);
    ESP_ERROR_CHECK(i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, SMBUS_DEFAULT_TIMEOUT));
    i2c_cmd_link_delete(cmd);

    // TODO return an actual error status
    return 0;
}

/**
 * @brief SMBus write block
 */
static esp_err_t smb_write_block(uint8_t command, uint8_t *data, uint8_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, TPS53647_I2CADDR << 1 | WRITE_BIT, ACK_CHECK);
    i2c_master_write_byte(cmd, command, ACK_CHECK);
    i2c_master_write_byte(cmd, len, ACK_CHECK);
    for (size_t i = 0; i < len; ++i)
    {
        i2c_master_write_byte(cmd, data[i], ACK_CHECK);
    }
    i2c_master_stop(cmd);
    i2c_set_timeout(I2C_MASTER_NUM, 20);
    ESP_ERROR_CHECK(i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, SMBUS_DEFAULT_TIMEOUT));
    i2c_cmd_link_delete(cmd);

    // TODO return an actual error status
    return 0;
}


uint8_t volt_to_vid(float volts) {
    if (volts == 0.0f) {
        return 0x00;
    }

    // get rounded fixed point
    int volts_fp = (int) (volts * 100.0f + 0.5f);
    int hw_min_volts_fp = (int) (HW_MIN_VOLTAGE * 100.0f + 0.5f);

    // Each step corresponds to 0.5V (i.e., 5 in the fixed-point representation)
    int register_value = (uint8_t)((volts_fp - hw_min_volts_fp) / 5) + 0x01;

    // Ensure the register value is within the valid range (0x01 to 0xFF)
    if (register_value > 0xFF) {
        printf("Error: Calculated register value out of range.\n");
        ESP_LOGI(TAG, "ERR - vout register value out of range %d", register_value);
        return 0;
    }
    return register_value;
}

float vid_to_volt(uint8_t register_value) {
    if (register_value == 0x00) {
        return 0.0f;
    }

    // Subtract the 0x01 offset
    int step = register_value - 0x01;

    // Each step corresponds to 0.005V (i.e., 5 in the fixed-point representation)
    int volts_fp = step * 5;

    // Add the hardware minimum voltage fixed-point value
    int hw_min_volts_fp = (int) (HW_MIN_VOLTAGE * 100.0f + 0.5f);
    volts_fp += hw_min_volts_fp;

    // Convert back to float
    float volts = volts_fp / 100.0f;

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
    return (int)result;
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
        for (i=0; i<=15; i++) {
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
        for (i=0; i<=15; i++) {
            mantissa = value * powf(2.0, i);
            if (mantissa >= 1024) {
                exponent = i-1;
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

    result = (( (~exponent + 1) << 11) & 0xF800) + mantissa;

    return result;
}


/*--- Public TPS53647 functions ---*/

// Set up the TPS53647 regulator and turn it on
int TPS53647_init(void)
{
    uint8_t u8_value;

    ESP_LOGI(TAG, "Initializing the core voltage regulator");

    /* Establish communication with regulator */
    uint16_t device_code;
    smb_read_word(PMBUS_MFR_SPECIFIC_44, &device_code);

    ESP_LOGI(TAG, "Device Code: %04x", device_code);

    if (device_code != 0x01f0)
    {
        ESP_LOGI(TAG, "ERROR- cannot find TPS53647 regulator");
        return -1;
    }

    /* set ON_OFF config, make sure the buck is switched off */
    smb_write_byte(PMBUS_ON_OFF_CONFIG, ON_OFF_CONFIG);
    smb_write_byte(PMBUS_OPERATION, OPERATION_OFF);

    // Switch frequency, 500kHz
    smb_write_byte(PMBUS_MFR_SPECIFIC_12, 0x20); // default value

    // set maximum current
    // with 100k on the IMON pin  the device should report the correct current
    // via PMBUS_READ_IOUT
    smb_write_byte(PMBUS_MFR_SPECIFIC_10, TPS43647_INIT_IMAX);

    // operation mode
    // VR12 Mode
    // Enable dynamic phase shedding
    // Slew Rate 0.68mV/us
    smb_write_byte(PMBUS_MFR_SPECIFIC_13, 0x89); // default value

    // 2 phase operation
    smb_write_byte(PMBUS_MFR_SPECIFIC_20, 0x01);

    ESP_LOGI(TAG, "---Writing new config values to TPS53647---");
    /* set up the ON_OFF_CONFIG */
    ESP_LOGI(TAG, "Setting ON_OFF_CONFIG");
    smb_write_byte(PMBUS_ON_OFF_CONFIG, TPS53647_INIT_ON_OFF_CONFIG);

    /* Switch frequency, 500kHz */
    ESP_LOGI(TAG, "Setting FREQUENCY");
    smb_write_byte(PMBUS_MFR_SPECIFIC_12, 0x20);

    // 2 phase operation
    smb_write_byte(PMBUS_MFR_SPECIFIC_20, 0x01);

    /* vout voltage */
    smb_write_word(PMBUS_VOUT_COMMAND, (uint16_t) volt_to_vid(TPS53647_INIT_VOUT_COMMAND));
    smb_write_word(PMBUS_VOUT_MAX, (uint16_t) volt_to_vid(TPS53647_INIT_VOUT_MAX));
    smb_write_word(PMBUS_VOUT_MARGIN_HIGH, (uint16_t) volt_to_vid(TPS53647_INIT_VOUT_MARGIN_HIGH));
    smb_write_word(PMBUS_VOUT_MARGIN_LOW, (uint16_t) volt_to_vid(TPS53647_INIT_VOUT_MARGIN_LOW));

    /* iout current */
    ESP_LOGI(TAG, "Setting IOUT");
    smb_write_word(PMBUS_IOUT_OC_WARN_LIMIT, float_2_slinear11(TPS53647_INIT_IOUT_OC_WARN_LIMIT));
    smb_write_word(PMBUS_IOUT_OC_FAULT_LIMIT, float_2_slinear11(TPS53647_INIT_IOUT_OC_FAULT_LIMIT));
    smb_write_byte(PMBUS_IOUT_OC_FAULT_RESPONSE, TPS53647_INIT_IOUT_OC_FAULT_RESPONSE);

    /* temperature */
    ESP_LOGI(TAG, "Setting TEMPERATURE");
    smb_write_word(PMBUS_OT_WARN_LIMIT, int_2_slinear11(TPS53647_INIT_OT_WARN_LIMIT));
    smb_write_word(PMBUS_OT_FAULT_LIMIT, int_2_slinear11(TPS53647_INIT_OT_FAULT_LIMIT));
    smb_write_byte(PMBUS_OT_FAULT_RESPONSE, TPS53647_INIT_OT_FAULT_RESPONSE);

    /* Show temperature */
    ESP_LOGI(TAG, "--------------------------------");
    ESP_LOGI(TAG, "Temp: %d", TPS53647_get_temperature());

    /* Show voltage settings */
    TPS53647_show_voltage_settings();

    ESP_LOGI(TAG, "-----------VOLTAGE/CURRENT---------------------");
    /* Get voltage input (SLINEAR11) */
    TPS53647_get_vin();
    /* Get output current (SLINEAR11) */
    TPS53647_get_iout();
    /* Get voltage output (VID) */
    TPS53647_get_vout();

    return 0;
}

int TPS53647_get_temperature(void)
{
    uint16_t value;
    int temp;

    smb_read_word(PMBUS_READ_TEMPERATURE_1, &value);
    temp = slinear11_2_int(value);
    return temp;
}

float TPS53647_get_pin(void)
{
    uint16_t u16_value;
    float pin;

    /* Get voltage input (SLINEAR11) */
    smb_read_word(PMBUS_READ_PIN, &u16_value);
    pin = slinear11_2_float(u16_value);
#ifdef _DEBUG_LOG_
    ESP_LOGI(TAG, "Got Pin: %2.3f V", pin);
#endif
    return pin;
}

float TPS53647_get_pout(void)
{
    uint16_t u16_value;
    float pout;

    /* Get voltage input (SLINEAR11) */
    smb_read_word(PMBUS_READ_POUT, &u16_value);
    pout = slinear11_2_float(u16_value);
#ifdef _DEBUG_LOG_
    ESP_LOGI(TAG, "Got Pout: %2.3f V", pout);
#endif
    return pout;
}


float TPS53647_get_vin(void)
{
    uint16_t u16_value;
    float vin;

    /* Get voltage input (SLINEAR11) */
    smb_read_word(PMBUS_READ_VIN, &u16_value);
    vin = slinear11_2_float(u16_value);
#ifdef _DEBUG_LOG_
    ESP_LOGI(TAG, "Got Vin: %2.3f V", vin);
#endif
    return vin;
}

float TPS53647_get_vout(void)
{
    uint16_t u16_value;
    float vout;

    smb_read_word(PMBUS_READ_VOUT, &u16_value);
    vout = vid_to_volt(u16_value);
#ifdef _DEBUG_LOG_
    ESP_LOGI(TAG, "Got Vout: %2.3f V", vout);
#endif
    return vout;
}

float TPS53647_get_iin(void)
{
    uint16_t u16_value;
    float iin;

    /* Get current output (SLINEAR11) */
    smb_read_word(PMBUS_READ_IOUT, &u16_value);
    iin = slinear11_2_float(u16_value);

#ifdef _DEBUG_LOG_
    ESP_LOGI(TAG, "Got Iin: %2.3f A", iin);
#endif

    return iin;
}

float TPS53647_get_iout(void)
{
    uint16_t u16_value;
    float iout;

    /* Get current output (SLINEAR11) */
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
        /* turn off output */
        smb_write_byte(PMBUS_OPERATION, OPERATION_OFF);
        return;
    }

    /* make sure we're in range */
    if ((volts < TPS53647_INIT_VOUT_MIN) || (volts > TPS53647_INIT_VOUT_MAX)) {
        ESP_LOGI(TAG, "ERR- Voltage requested (%f V) is out of range", volts);
        return;
    }

    /* set output voltage */
    smb_write_word(PMBUS_VOUT_COMMAND, (uint16_t) volt_to_vid(volts));

    /* turn on output */
    smb_write_byte(PMBUS_OPERATION, OPERATION_ON);

    ESP_LOGI(TAG, "Vout changed to %1.2f V", volts);
}

void TPS53647_show_voltage_settings(void)
{
    uint16_t u16_value;
    float f_value;

    ESP_LOGI(TAG, "-----------VOLTAGE---------------------");

    /* VOUT_MAX */
    smb_read_word(PMBUS_VOUT_MAX, &u16_value);
    f_value = vid_to_volt(u16_value);
    ESP_LOGI(TAG, "Vout Max set to: %f V", f_value);

    /* VOUT_MARGIN_HIGH */
    smb_read_word(PMBUS_VOUT_MARGIN_HIGH, &u16_value);
    f_value = vid_to_volt(u16_value);
    ESP_LOGI(TAG, "Vout Margin HIGH: %f V", f_value);

    /* --- VOUT_COMMAND --- */
    smb_read_word(PMBUS_VOUT_COMMAND, &u16_value);
    f_value = vid_to_volt(u16_value);
    ESP_LOGI(TAG, "Vout set to: %f V", f_value);

    /* VOUT_MARGIN_LOW */
    smb_read_word(PMBUS_VOUT_MARGIN_LOW, &u16_value);
    f_value = vid_to_volt(u16_value);
    ESP_LOGI(TAG, "Vout Margin LOW: %f V", f_value);

}

