#include <stdio.h>

#include "esp_log.h"

#include "i2c_master.h"
#include "TMP1075.h"

static const char *TAG = "TMP1075.c";

#define I2C_MASTER_NUM ((i2c_port_t) 0)

#define WRITE_BIT I2C_MASTER_WRITE
#define READ_BIT I2C_MASTER_READ
#define ACK_CHECK true
#define ACK_VALUE ((i2c_ack_type_t) 0x0)
#define NACK_VALUE ((i2c_ack_type_t) 0x1)
#define MAX_BLOCK_LEN 32



// don't ask me why simpler single-line calls didn't work :shrug:
static esp_err_t TMP1075_smb_read_word(uint8_t device, uint8_t reg, uint16_t *result)
{
    uint8_t data[2] = {0, 0};
    esp_err_t err;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (TMP1075_I2CADDR_DEFAULT + device) << 1 | WRITE_BIT, ACK_CHECK);
    i2c_master_write_byte(cmd, reg, ACK_CHECK);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (TMP1075_I2CADDR_DEFAULT + device) << 1 | READ_BIT, ACK_CHECK);
    i2c_master_read(cmd, data, 2, ACK_VALUE);
    i2c_master_stop(cmd);

    err = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    if (err == ESP_OK) {
        *result = (data[0] << 8) | data[1];
    }

    return err;
}

float TMP1075_read_temperature(int device_index)
{
    uint16_t temp_raw = 0;
    int ret = TMP1075_smb_read_word(device_index, TMP1075_TEMP_REG, &temp_raw);

    if (ret != ESP_OK) {
        ESP_LOGI(TAG, "Failed to read temperature from TMP1075");
        return 0.0f;
    }

    // check for invalid reading
    if (temp_raw > 0x7ff0) {
        ESP_LOGI(TAG, "Invalid TMP1075 reading: %04x", temp_raw);
        return 0.0f;
    }

    return (temp_raw >> 4) * 0.0625f; // Each bit represents 0.0625°C
}
