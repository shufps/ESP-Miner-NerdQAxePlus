#include <stdio.h>
#include "esp_log.h"
#include "i2c_master.h"

#include "TMP1075.h"

static const char *TAG = "TMP1075.c";


float TMP1075_read_temperature(int device_index)
{
    uint8_t data[2];
    int ret = i2c_master_write_read_device(I2C_MASTER_NUM, TMP1075_I2CADDR_DEFAULT + device_index,
                                           TMP1075_TEMP_REG, 1, data, 2, 1000 / portTICK_PERIOD_MS);
    if (ret == ESP_OK) {
        int16_t temp_raw = (data[0] << 8) | data[1]; // Combine the two bytes
        temp_raw >>= 4; // Right-shift to discard the unused bits (12-bit data)
        float temperature = temp_raw * 0.0625f; // Each bit represents 0.0625°C
        ESP_LOGI(TAG, "Temperature %d: %.2f C", device_index, temperature);
        return temperature;
    } else {
        ESP_LOGI(TAG, "Failed to read temperature from TMP1075");
        return 0.0f;
    }
}

