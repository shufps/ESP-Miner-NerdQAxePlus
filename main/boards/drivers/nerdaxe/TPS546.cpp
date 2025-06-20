#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include "esp_log.h"
#include "pmbus_commands.h"

#include "i2c_master.h"
#include "TPS546.h"

//#define _DEBUG_LOG_ 1

//#define I2C_MASTER_NUM 0 /*!< I2C master i2c port number, the number of i2c peripheral interfaces available will depend on the chip */

#define WRITE_BIT      I2C_MASTER_WRITE
#define READ_BIT       I2C_MASTER_READ
#define ACK_CHECK      true
#define NO_ACK_CHECK   false
#define ACK_VALUE ((i2c_ack_type_t) 0x0)
#define NACK_VALUE ((i2c_ack_type_t) 0x1)
#define MAX_BLOCK_LEN  32

#define SMBUS_DEFAULT_TIMEOUT pdMS_TO_TICKS(1000)

static const char *TAG = "TPS546";

static uint8_t DEVICE_ID1[] = {0x54, 0x49, 0x54, 0x6B, 0x24, 0x41}; // TPS546D24A
static uint8_t DEVICE_ID2[] = {0x54, 0x49, 0x54, 0x6D, 0x24, 0x41}; // TPS546D24A
static uint8_t DEVICE_ID3[] = {0x54, 0x49, 0x54, 0x6D, 0x24, 0x62}; // TPS546D24S
static uint8_t MFR_ID[] = {'B', 'A', 'X'};
static uint8_t MFR_MODEL[] = {'H', 'E', 'X'};
static uint8_t MFR_REVISION[] = {0x00, 0x00, 0x01};

static bool is_initialized = false;

//static uint8_t COMPENSATION_CONFIG[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

//static i2c_master_dev_handle_t tps546_dev_handle;

/**
 * @brief SMBus read byte
 */
static esp_err_t smb_read_byte(uint8_t command, uint8_t *data)
{
    esp_err_t err = ESP_FAIL;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, TPS546_I2CADDR << 1 | WRITE_BIT, ACK_CHECK);
    i2c_master_write_byte(cmd, command, ACK_CHECK);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, TPS546_I2CADDR << 1 | READ_BIT, ACK_CHECK);
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
    i2c_master_write_byte(cmd, TPS546_I2CADDR << 1 | WRITE_BIT, ACK_CHECK);
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
    i2c_master_write_byte(cmd, TPS546_I2CADDR << 1 | WRITE_BIT, ACK_CHECK);
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
    i2c_master_write_byte(cmd, TPS546_I2CADDR << 1 | WRITE_BIT, ACK_CHECK);
    i2c_master_write_byte(cmd, command, ACK_CHECK);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, TPS546_I2CADDR << 1 | READ_BIT, ACK_CHECK);
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
    i2c_master_write_byte(cmd, TPS546_I2CADDR << 1 | WRITE_BIT, ACK_CHECK);
    i2c_master_write_byte(cmd, command, ACK_CHECK);
    i2c_master_write_byte(cmd, (uint8_t) (data & 0x00FF), ACK_CHECK);
    i2c_master_write_byte(cmd, (uint8_t) ((data & 0xFF00) >> 8), NACK_VALUE);
    i2c_master_stop(cmd);
    err = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, SMBUS_DEFAULT_TIMEOUT);
    i2c_cmd_link_delete(cmd);

    return err;
}

/**
 * @brief SMBus read block -- SMBus is funny in that the first byte returned is the length of data??
 * @param command The command to read
 * @param data Pointer to store the read data
 * @param len The number of bytes to read
 */
static esp_err_t smb_read_block(uint8_t command, uint8_t *data, uint8_t len)
{
    //malloc a buffer len+1 to store the length byte
    uint8_t *buf = (uint8_t *)malloc(len+1);
    if (i2c_master_register_read(TPS546_I2CADDR, command, buf, len+1) != ESP_OK) {
        free(buf);
        return ESP_FAIL;
    }
    //copy the data into the buffer
    memcpy(data, buf+1, len);
    free(buf);

    return ESP_OK;
}

/**
 * @brief SMBus write block - don;t forget the length byte first :P
 * @param command The command to write
 * @param data The data to write
 * @param len The number of bytes to write
 */
static esp_err_t smb_write_block(uint8_t command, uint8_t *data, uint8_t len)
{
    //malloc a buffer len+2 to store the command byte and then the length byte
    uint8_t *buf = (uint8_t *)malloc(len+2);
    buf[0] = command;
    buf[1] = len;
    //copy the data into the buffer
    memcpy(buf+2, data, len);

    //write it all
    //if (i2c_master_register_write_word(TPS546_I2CADDR, buf, len+2) != ESP_OK) {
    if (i2c_master_register_write_word(TPS546_I2CADDR, buf[0], buf[1] << 8 | buf[2]) != ESP_OK) {
        free(buf);
        return ESP_FAIL;
    } else {
        free(buf);
        return ESP_OK;
    }
}

/**
 * @brief Convert an SLINEAR11 value into an int
 * @param value The SLINEAR11 value to convert
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
 * @param value The SLINEAR11 value to convert
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
    result = (float) mantissa * powf(2.0, (float) exponent);
    return result;
}

/**
 * @brief Convert an int value into an SLINEAR11
 * @param value The int value to convert
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
 * @param value The float value to convert
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

/**
 * @brief Convert a ULINEAR16 value into a float
 * the exponent comes from the VOUT_MODE bits[4..0]
 * stored in twos-complement
 * The mantissa occupies the full 16-bits of the value
 * @param value The ULINEAR16 value to convert
 */
static float ulinear16_2_float(uint16_t value)
{
    uint8_t voutmode;
    int exponent;
    float result;

    smb_read_byte(PMBUS_VOUT_MODE, &voutmode);

    if (voutmode & 0x10) {
        // exponent is negative
        exponent = -1 * ((~voutmode & 0x1F) + 1);
    } else {
        exponent = (voutmode & 0x1F);
    }
    result = (value * powf(2.0, exponent));
    return result;
}

/**
 * @brief Convert a float value into a ULINEAR16
 * the exponent comes from the VOUT_MODE bits[4..0]
 * stored in twos-complement
 * The mantissa occupies the full 16-bits of the result
 * @param value The float value to convert
*/
static uint16_t float_2_ulinear16(float value)
{
    uint8_t voutmode;
    float exponent;
    uint16_t result;

    smb_read_byte(PMBUS_VOUT_MODE, &voutmode);
    if (voutmode & 0x10) {
        // exponent is negative
        exponent = -1 * ((~voutmode & 0x1F) + 1);
    } else {
        exponent = (voutmode & 0x1F);
    }

    result = (value / powf(2.0, exponent));

    return result;
}

/*--- Public TPS546 functions ---*/

/**
 * @brief Set up the TPS546 regulator and turn it on
*/
int TPS546_init(void)
{
	uint8_t data[7];
    uint8_t u8_value;
    uint16_t u16_value;
    uint8_t read_mfr_revision[4];
    int temp;
    uint8_t comp_config[5];
    uint8_t voutmode;

    ESP_LOGI(TAG, "Initializing the core voltage regulator");

    /*if (i2c_master_add_device(TPS546_I2CADDR, &tps546_dev_handle, TAG) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add I2C device");
        return -1;
    }*/

    /* Establish communication with regulator */
    smb_read_block(PMBUS_IC_DEVICE_ID, data, 6); //the DEVICE_ID block first byte is the length.
    ESP_LOGI(TAG, "Device ID: %02x %02x %02x %02x %02x %02x", data[0], data[1], data[2], data[3], data[4], data[5]);
    /* There's 3 different known device IDs observed so far */
    if ( (memcmp(data, DEVICE_ID1, 6) != 0) && (memcmp(data, DEVICE_ID2, 6) != 0) && (memcmp(data, DEVICE_ID3, 6) != 0))
    {
        ESP_LOGE(TAG, "Cannot find TPS546 regulator - Device ID mismatch");
        return -1;
    }

    /* Make sure power is turned off until commanded */
    u8_value = ON_OFF_CONFIG_CMD | ON_OFF_CONFIG_PU | ON_OFF_CONFIG_CP |
            ON_OFF_CONFIG_POLARITY | ON_OFF_CONFIG_DELAY;
    ESP_LOGI(TAG, "Power config-ON_OFF_CONFIG: %02x", u8_value);
    smb_write_byte(PMBUS_ON_OFF_CONFIG, u8_value);

    /* Read version number and see if it matches */
    TPS546_read_mfr_info(read_mfr_revision);
    // if (memcmp(read_mfr_revision, MFR_REVISION, 3) != 0) {

    // If it doesn't match, then write all the registers and set new version number
    // ESP_LOGI(TAG, "--------------------------------");
    // ESP_LOGI(TAG, "Config version mismatch, writing new config values");
    ESP_LOGI(TAG, "Writing new config values");
    smb_read_byte(PMBUS_VOUT_MODE, &voutmode);
    ESP_LOGI(TAG, "VOUT_MODE: %02x", voutmode);
    TPS546_write_entire_config();
    //}

    // /* Show temperature */
    // ESP_LOGI(TAG, "--------------------------------");
    // ESP_LOGI(TAG, "Temp: %d", TPS546_get_temperature());

    // /* Show switching frequency */
    // TPS546_get_frequency();
    // TPS546_set_frequency(650);

    /* Show voltage settings */
    TPS546_show_voltage_settings();

    //TPS546_print_status();

    // ESP_LOGI(TAG, "-----------VOLTAGE/CURRENT---------------------");
    // /* Get voltage input (SLINEAR11) */
    // ESP_LOGI(TAG, "READ_VIN: %.2fV", TPS546_get_vin());
    // /* Get output current (SLINEAR11) */
    // ESP_LOGI(TAG, "READ_IOUT: %.2fA", TPS546_get_iout());
    // /* Get voltage output (ULINEAR16) */
    // ESP_LOGI(TAG, "READ_VOUT: %.2fV", TPS546_get_vout());

    ESP_LOGI(TAG, "-----------TIMING---------------------");
    smb_read_word(PMBUS_TON_DELAY, &u16_value);
    temp = slinear11_2_int(u16_value);
    ESP_LOGI(TAG, "TON_DELAY: %d", temp);
    smb_read_word(PMBUS_TON_RISE, &u16_value);
    temp = slinear11_2_int(u16_value);
    ESP_LOGI(TAG, "TON_RISE: %d", temp);
    smb_read_word(PMBUS_TON_MAX_FAULT_LIMIT, &u16_value);
    temp = slinear11_2_int(u16_value);
    ESP_LOGI(TAG, "TON_MAX_FAULT_LIMIT: %d", temp);
    smb_read_byte(PMBUS_TON_MAX_FAULT_RESPONSE, &u8_value);
    ESP_LOGI(TAG, "TON_MAX_FAULT_RESPONSE: %02x", u8_value);
    smb_read_word(PMBUS_TOFF_DELAY, &u16_value);
    temp = slinear11_2_int(u16_value);
    ESP_LOGI(TAG, "TOFF_DELAY: %d", temp);
    smb_read_word(PMBUS_TOFF_FALL, &u16_value);
    temp = slinear11_2_int(u16_value);
    ESP_LOGI(TAG, "TOFF_FALL: %d", temp);
    ESP_LOGI(TAG, "--------------------------------------");

    // Read the compensation config registers
    if (smb_read_block(PMBUS_COMPENSATION_CONFIG, comp_config, 5) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read COMPENSATION CONFIG");
        return -1;
    }
    ESP_LOGI(TAG, "COMPENSATION CONFIG");
    ESP_LOGI(TAG, "%02x %02x %02x %02x %02x", comp_config[0], comp_config[1],
        comp_config[2], comp_config[3], comp_config[4]);

    is_initialized = true;

    return 0;
}

/**
 * @brief Read the manufacturer model and revision
 * @param read_mfr_revision Pointer to store the read revision
*/
void TPS546_read_mfr_info(uint8_t *read_mfr_revision)
{
    uint8_t read_mfr_id[4];
    uint8_t read_mfr_model[4];

    ESP_LOGI(TAG, "Reading MFR info");
    if (smb_read_block(PMBUS_MFR_ID, read_mfr_id, 3) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read MFR ID");
        return;
    }
    read_mfr_id[3] = 0x00;
    if (smb_read_block(PMBUS_MFR_MODEL, read_mfr_model, 3) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read MFR MODEL");
        return;
    }
    read_mfr_model[3] = 0x00;
    if (smb_read_block(PMBUS_MFR_REVISION, read_mfr_revision, 3) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read MFR REVISION");
        return;
    }

    ESP_LOGI(TAG, "MFR_ID: %s", read_mfr_id);
    ESP_LOGI(TAG, "MFR_MODEL: %s", read_mfr_model);
    ESP_LOGI(TAG, "MFR_REVISION: %d%d%d ", read_mfr_revision[0],
        read_mfr_revision[1], read_mfr_revision[2]);
}

/**
 * @brief Write the manufacturer ID and revision to NVM
*/
void TPS546_set_mfr_info(void)
{
    ESP_LOGI(TAG, "Setting MFR info");
	smb_write_block(PMBUS_MFR_ID, MFR_ID, 3);
	smb_write_block(PMBUS_MFR_MODEL, MFR_MODEL, 3);
	smb_write_block(PMBUS_MFR_REVISION, MFR_REVISION, 3);
}

/**
 * @brief Set all the relevant config registers for normal operation
*/
void TPS546_write_entire_config(void)
{
    ESP_LOGI(TAG, "---Writing new config values to TPS546---");
    /* set up the ON_OFF_CONFIG */
    ESP_LOGI(TAG, "Setting ON_OFF_CONFIG");
    if (smb_write_byte(PMBUS_ON_OFF_CONFIG, TPS546_INIT_ON_OFF_CONFIG) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write ON_OFF_CONFIG");
        return;
    }

    /* Switch frequency */
    ESP_LOGI(TAG, "Setting FREQUENCY");
    smb_write_word(PMBUS_FREQUENCY_SWITCH, int_2_slinear11(TPS546_INIT_FREQUENCY));

    /* vin voltage */
    ESP_LOGI(TAG, "Setting VIN_ON: %.2f", TPS546_INIT_VIN_ON);
    smb_write_word(PMBUS_VIN_ON, float_2_slinear11(TPS546_INIT_VIN_ON));
    ESP_LOGI(TAG, "Setting VIN_OFF: %.2f", TPS546_INIT_VIN_OFF);
    smb_write_word(PMBUS_VIN_OFF, float_2_slinear11(TPS546_INIT_VIN_OFF));
    ESP_LOGI(TAG, "Setting VIN_UV_WARN_LIMIT: %.2f", TPS546_INIT_VIN_UV_WARN_LIMIT);
    smb_write_word(PMBUS_VIN_UV_WARN_LIMIT, float_2_slinear11(TPS546_INIT_VIN_UV_WARN_LIMIT));
    ESP_LOGI(TAG, "Setting VIN_OV_FAULT_LIMIT: %.2f", TPS546_INIT_VIN_OV_FAULT_LIMIT);
    smb_write_word(PMBUS_VIN_OV_FAULT_LIMIT, float_2_slinear11(TPS546_INIT_VIN_OV_FAULT_LIMIT));
    ESP_LOGI(TAG, "Setting VIN_OV_FAULT_RESPONSE: %02X", TPS546_INIT_VIN_OV_FAULT_RESPONSE);
    smb_write_byte(PMBUS_VIN_OV_FAULT_RESPONSE, TPS546_INIT_VIN_OV_FAULT_RESPONSE);

    /* vout voltage */
    ESP_LOGI(TAG, "Setting VOUT SCALE: %.2f", TPS546_INIT_SCALE_LOOP);
    smb_write_word(PMBUS_VOUT_SCALE_LOOP, float_2_slinear11(TPS546_INIT_SCALE_LOOP));
    ESP_LOGI(TAG, "VOUT_COMMAND: %.2f", TPS546_INIT_VOUT_COMMAND);
    smb_write_word(PMBUS_VOUT_COMMAND, float_2_ulinear16(TPS546_INIT_VOUT_COMMAND));
    ESP_LOGI(TAG, "VOUT_MAX: %d", TPS546_INIT_VOUT_MAX);
    smb_write_word(PMBUS_VOUT_MAX, float_2_ulinear16(TPS546_INIT_VOUT_MAX));
    ESP_LOGI(TAG, "VOUT_OV_FAULT_LIMIT: %.2f", TPS546_INIT_VOUT_OV_FAULT_LIMIT);
    smb_write_word(PMBUS_VOUT_OV_FAULT_LIMIT, float_2_ulinear16(TPS546_INIT_VOUT_OV_FAULT_LIMIT));
    ESP_LOGI(TAG, "VOUT_OV_WARN_LIMIT: %.2f", TPS546_INIT_VOUT_OV_WARN_LIMIT);
    smb_write_word(PMBUS_VOUT_OV_WARN_LIMIT, float_2_ulinear16(TPS546_INIT_VOUT_OV_WARN_LIMIT));
    ESP_LOGI(TAG, "VOUT_MARGIN_HIGH: %.2f", TPS546_INIT_VOUT_MARGIN_HIGH);
    smb_write_word(PMBUS_VOUT_MARGIN_HIGH, float_2_ulinear16(TPS546_INIT_VOUT_MARGIN_HIGH));
    ESP_LOGI(TAG, "VOUT_MARGIN_LOW: %.2f", TPS546_INIT_VOUT_MARGIN_LOW);
    smb_write_word(PMBUS_VOUT_MARGIN_LOW, float_2_ulinear16(TPS546_INIT_VOUT_MARGIN_LOW));
    ESP_LOGI(TAG, "VOUT_UV_WARN_LIMIT: %.2f", TPS546_INIT_VOUT_UV_WARN_LIMIT);
    smb_write_word(PMBUS_VOUT_UV_WARN_LIMIT, float_2_ulinear16(TPS546_INIT_VOUT_UV_WARN_LIMIT));
    ESP_LOGI(TAG, "VOUT_UV_FAULT_LIMIT: %.2f", TPS546_INIT_VOUT_UV_FAULT_LIMIT);
    smb_write_word(PMBUS_VOUT_UV_FAULT_LIMIT, float_2_ulinear16(TPS546_INIT_VOUT_UV_FAULT_LIMIT));
    ESP_LOGI(TAG, "VOUT_MIN: %d", TPS546_INIT_VOUT_MIN);
    smb_write_word(PMBUS_VOUT_MIN, float_2_ulinear16(TPS546_INIT_VOUT_MIN));

    /* iout current */
    ESP_LOGI(TAG, "Setting IOUT");
    smb_write_word(PMBUS_IOUT_OC_WARN_LIMIT, float_2_slinear11(TPS546_INIT_IOUT_OC_WARN_LIMIT));
    smb_write_word(PMBUS_IOUT_OC_FAULT_LIMIT, float_2_slinear11(TPS546_INIT_IOUT_OC_FAULT_LIMIT));
    smb_write_byte(PMBUS_IOUT_OC_FAULT_RESPONSE, TPS546_INIT_IOUT_OC_FAULT_RESPONSE);

    /* temperature */
    ESP_LOGI(TAG, "Setting TEMPERATURE");
    ESP_LOGI(TAG, "OT_WARN_LIMIT: %d", TPS546_INIT_OT_WARN_LIMIT);
    smb_write_word(PMBUS_OT_WARN_LIMIT, int_2_slinear11(TPS546_INIT_OT_WARN_LIMIT));
    ESP_LOGI(TAG, "OT_FAULT_LIMIT: %d", TPS546_INIT_OT_FAULT_LIMIT);
    smb_write_word(PMBUS_OT_FAULT_LIMIT, int_2_slinear11(TPS546_INIT_OT_FAULT_LIMIT));
    ESP_LOGI(TAG, "OT_FAULT_RESPONSE: %02x", TPS546_INIT_OT_FAULT_RESPONSE);
    smb_write_byte(PMBUS_OT_FAULT_RESPONSE, TPS546_INIT_OT_FAULT_RESPONSE);

    /* timing */
    ESP_LOGI(TAG, "Setting TIMING");
    ESP_LOGI(TAG, "TON_DELAY: %d", TPS546_INIT_TON_DELAY);
    smb_write_word(PMBUS_TON_DELAY, int_2_slinear11(TPS546_INIT_TON_DELAY));
    ESP_LOGI(TAG, "TON_RISE: %d", TPS546_INIT_TON_RISE);
    smb_write_word(PMBUS_TON_RISE, int_2_slinear11(TPS546_INIT_TON_RISE));
    ESP_LOGI(TAG, "TON_MAX_FAULT_LIMIT: %d", TPS546_INIT_TON_MAX_FAULT_LIMIT);
    smb_write_word(PMBUS_TON_MAX_FAULT_LIMIT, int_2_slinear11(TPS546_INIT_TON_MAX_FAULT_LIMIT));
    ESP_LOGI(TAG, "TON_MAX_FAULT_RESPONSE: %02x", TPS546_INIT_TON_MAX_FAULT_RESPONSE);
    smb_write_byte(PMBUS_TON_MAX_FAULT_RESPONSE, TPS546_INIT_TON_MAX_FAULT_RESPONSE);
    ESP_LOGI(TAG, "TOFF_DELAY: %d", TPS546_INIT_TOFF_DELAY);
    smb_write_word(PMBUS_TOFF_DELAY, int_2_slinear11(TPS546_INIT_TOFF_DELAY));
    ESP_LOGI(TAG, "TOFF_FALL: %d", TPS546_INIT_TOFF_FALL);
    smb_write_word(PMBUS_TOFF_FALL, int_2_slinear11(TPS546_INIT_TOFF_FALL));

    /* Compensation config */
    //ESP_LOGI(TAG, "COMPENSATION");
    //smb_write_block(PMBUS_COMPENSATION_CONFIG, COMPENSATION_CONFIG, 5);

    /* configure the bootup behavior regarding pin detect values vs NVM values */
    ESP_LOGI(TAG, "Setting PIN_DETECT_OVERRIDE");
    smb_write_word(PMBUS_PIN_DETECT_OVERRIDE, INIT_PIN_DETECT_OVERRIDE);

    /* TODO write new MFR_REVISION number to reflect these parameters */
    ESP_LOGI(TAG, "Writing MFR ID");
    smb_write_block(PMBUS_MFR_ID, MFR_ID, 3);
    ESP_LOGI(TAG, "Writing MFR MODEL");
    smb_write_block(PMBUS_MFR_ID, MFR_MODEL, 3);
    ESP_LOGI(TAG, "Writing MFR REVISION");
    smb_write_block(PMBUS_MFR_ID, MFR_REVISION, 3);

    /*
    !!!!!!!!!!!!!!!!!!!!!!!!!!!
    // Never write this to NVM as it can corrupt the TPS in an unrecoverable state, just do it on boot every time
    !!!!!!!!!!!!!!!!!!!!!!!!!!!
    */
    /* store configuration in NVM */
    // ESP_LOGI(TAG, "---Saving new config---");
    // smb_write_byte(PMBUS_STORE_USER_ALL, 0x98);

}

int TPS546_get_frequency(void)
{
    uint16_t value;
    int freq;

    smb_read_word(PMBUS_FREQUENCY_SWITCH, &value);
    freq = slinear11_2_int(value);

    return (int)freq;
}

void TPS546_set_frequency(int newfreq)
{
    uint16_t value;
    //int freq;

    ESP_LOGI(TAG, "Writing new frequency: %d", newfreq);
    value = int_2_slinear11(newfreq);
    //ESP_LOGI(TAG, "New value: 0x%04x", value);
    smb_write_word(PMBUS_FREQUENCY_SWITCH, value);

    //ESP_LOGI(TAG, "Checking conversion...");
    //freq = slinear11_2_int(value);
    //ESP_LOGI(TAG, "Converted value: %d", freq);
}

float TPS546_get_temperature(void)
{
    uint16_t value;

    if (!is_initialized) {
        return 0;
    }

    smb_read_word(PMBUS_READ_TEMPERATURE_1, &value);

    return slinear11_2_float(value);
}

float TPS546_get_vin(void)
{
    uint16_t u16_value;
    float vin;

    if (!is_initialized) {
        return 0.0f;
    }

    /* Get voltage input (ULINEAR16) */
    if (smb_read_word(PMBUS_READ_VIN, &u16_value) != ESP_OK) {
        ESP_LOGE(TAG, "Could not read VIN");
        return 0;
    } else {
        vin = slinear11_2_float(u16_value);
        #ifdef _DEBUG_LOG_
        ESP_LOGI(TAG, "Got Vin: %2.3f V", vin);
        #endif
        return vin;
    }
}

float TPS546_get_iout(void)
{
    uint16_t u16_value;
    float iout;

    if (!is_initialized) {
        return 0.0f;
    }

    /* Get current output (SLINEAR11) */
    if (smb_read_word(PMBUS_READ_IOUT, &u16_value) != ESP_OK) {
        ESP_LOGE(TAG, "Could not read Iout");
        return 0;
    } else {
        iout = slinear11_2_float(u16_value);

    //iout = iout * -1.0;
    #ifdef _DEBUG_LOG_
        ESP_LOGI(TAG, "Got Iout: %2.3f A", iout);
    #endif

        return iout;
}
}

float TPS546_get_vout(void)
{
    uint16_t u16_value;
    float vout;

    if (!is_initialized) {
        return 0.0f;
    }

    /* Get voltage output (ULINEAR16) */
    if (smb_read_word(PMBUS_READ_VOUT, &u16_value) != ESP_OK) {
        ESP_LOGE(TAG, "Could not read Vout");
        return 0;
    } else {
        vout = ulinear16_2_float(u16_value);
    #ifdef _DEBUG_LOG_
        ESP_LOGI(TAG, "Got Vout: %2.3f V", vout);
    #endif
        return vout;
    }
}

void TPS546_print_status(void) {
    uint16_t u16_value;
    uint8_t u8_value;

    if (smb_read_word(PMBUS_STATUS_WORD, &u16_value) != ESP_OK) {
        ESP_LOGE(TAG, "Could not read STATUS_WORD");
    } else {
        ESP_LOGI(TAG, "TPS546 Status: %04X", u16_value);
    }

    if (smb_read_byte(PMBUS_STATUS_VOUT, &u8_value) != ESP_OK) {
        ESP_LOGE(TAG, "Could not read STATUS_VOUT");
    } else {
        ESP_LOGI(TAG, "TPS546 VOUT Status: %02X", u8_value);
    }

    if (smb_read_byte(PMBUS_STATUS_INPUT, &u8_value) != ESP_OK) {
        ESP_LOGE(TAG, "Could not read STATUS_INPUT");
    } else {
        ESP_LOGI(TAG, "TPS546 INPUT Status: %02X", u8_value);
    }
}

/**
 * @brief Sets the core voltage
 * this function controls the regulator ontput state
 * send it the desired output in millivolts
 * A value between TPS546_INIT_VOUT_MIN and TPS546_INIT_VOUT_MAX
 * send a 0 to turn off the output
 * @param volts The desired output voltage
**/
void TPS546_set_vout(float volts)
{
    uint16_t value;

    if (volts == 0) {
        /* turn off output */
        if (smb_write_byte(PMBUS_OPERATION, OPERATION_OFF) != ESP_OK) {
            ESP_LOGE(TAG, "Could not turn off Vout");
        }
    } else {
        /* make sure we're in range */
        if ((volts < TPS546_INIT_VOUT_MIN) || (volts > TPS546_INIT_VOUT_MAX)) {
            ESP_LOGE(TAG, "Voltage requested (%f V) is out of range", volts);
        } else {
            /* set the output voltage */
            value = float_2_ulinear16(volts);
            if (smb_write_word(PMBUS_VOUT_COMMAND, value) != ESP_OK) {
                ESP_LOGE(TAG, "Could not set Vout to %1.2f V", volts);
            }

            ESP_LOGI(TAG, "Vout changed to %1.2f V", volts);

            /* turn on output */
           if (smb_write_byte(PMBUS_OPERATION, OPERATION_ON) != ESP_OK) {
                ESP_LOGE(TAG, "Could not turn on Vout");
            }
        }
    }
}

void TPS546_show_voltage_settings(void)
{
    uint16_t u16_value;
    float f_value;

    ESP_LOGI(TAG, "-----------VOLTAGE---------------------");
    /* VIN_ON SLINEAR11 */
    smb_read_word(PMBUS_VIN_ON, &u16_value);
    f_value = slinear11_2_float(u16_value);
    ESP_LOGI(TAG, "VIN ON set to: %.2f", f_value);

    /* VIN_OFF SLINEAR11 */
    smb_read_word(PMBUS_VIN_OFF, &u16_value);
    f_value = slinear11_2_float(u16_value);
    ESP_LOGI(TAG, "VIN OFF set to: %.2f", f_value);

    /* VOUT_MAX */
    smb_read_word(PMBUS_VOUT_MAX, &u16_value);
    f_value = ulinear16_2_float(u16_value);
    ESP_LOGI(TAG, "Vout Max set to: %.2f V", f_value);

    /* VOUT_OV_FAULT_LIMIT */
    smb_read_word(PMBUS_VOUT_OV_FAULT_LIMIT, &u16_value);
    f_value = ulinear16_2_float(u16_value);
    ESP_LOGI(TAG, "Vout OV Fault Limit: %.2f V", f_value);

    /* VOUT_OV_WARN_LIMIT */
    smb_read_word(PMBUS_VOUT_OV_WARN_LIMIT, &u16_value);
    f_value = ulinear16_2_float(u16_value);
    ESP_LOGI(TAG, "Vout OV Warn Limit: %.2f V", f_value);

    /* VOUT_MARGIN_HIGH */
    smb_read_word(PMBUS_VOUT_MARGIN_HIGH, &u16_value);
    f_value = ulinear16_2_float(u16_value);
    ESP_LOGI(TAG, "Vout Margin HIGH: %.2f V", f_value);

    /* --- VOUT_COMMAND --- */
    smb_read_word(PMBUS_VOUT_COMMAND, &u16_value);
    f_value = ulinear16_2_float(u16_value);
    ESP_LOGI(TAG, "Vout set to: %.2f V", f_value);

    /* VOUT_MARGIN_LOW */
    smb_read_word(PMBUS_VOUT_MARGIN_LOW, &u16_value);
    f_value = ulinear16_2_float(u16_value);
    ESP_LOGI(TAG, "Vout Margin LOW: %.2f V", f_value);

    /* VOUT_UV_WARN_LIMIT */
    smb_read_word(PMBUS_VOUT_UV_WARN_LIMIT, &u16_value);
    f_value = ulinear16_2_float(u16_value);
    ESP_LOGI(TAG, "Vout UV Warn Limit: %.2f V", f_value);

    /* VOUT_UV_FAULT_LIMIT */
    smb_read_word(PMBUS_VOUT_UV_FAULT_LIMIT, &u16_value);
    f_value = ulinear16_2_float(u16_value);
    ESP_LOGI(TAG, "Vout UV Fault Limit: %.2f V", f_value);

    /* VOUT_MIN */
    smb_read_word(PMBUS_VOUT_MIN, &u16_value);
    f_value = ulinear16_2_float(u16_value);
    ESP_LOGI(TAG, "Vout Min set to: %.2f V", f_value);
}

