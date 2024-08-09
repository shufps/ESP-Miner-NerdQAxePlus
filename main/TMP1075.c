#include <stdio.h>
#include "esp_log.h"
#include "i2c_master.h"

#include "TMP1075.h"

static const char *TAG = "TMP1075.c";

bool TMP1075_installed(int device_index)
{
    uint8_t data[2];
    esp_err_t result = ESP_OK;

    // read the configuration register
    //ESP_LOGI(TAG, "Reading configuration register");
    ESP_ERROR_CHECK(i2c_master_register_read(TMP1075_I2CADDR_DEFAULT + device_index, TMP1075_CONFIG_REG, data, 2));
    //ESP_LOGI(TAG, "Configuration[%d] = %02X %02X", device_index, data[0], data[1]);

    return (result == ESP_OK?true:false);
}

float TMP1075_read_temperature(int device_index)
{
    uint8_t data[2];

    ESP_ERROR_CHECK(i2c_master_register_read(TMP1075_I2CADDR_DEFAULT + device_index, TMP1075_TEMP_REG, data, 2));

    int temp_data = ((int) data[0] << 4) | ((int) data[1] >> 4);

    if (temp_data > 2047) {
        temp_data -= 4096;
    }

    float ftemp = (float) temp_data * 0.0625;

    //ESP_LOGI(TAG, "Raw Temperature = %02X %02X", data[0], data[1]);
    //ESP_LOGI(TAG, "Temperature[%d] = %d", device_index, ftemp);

    return ftemp;
}

