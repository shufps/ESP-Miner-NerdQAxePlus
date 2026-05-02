#include "can_task.h"

#include "driver/twai.h"
#include "esp_log.h"

static const char *TAG = "can";

#define CAN_TX_GPIO GPIO_NUM_1
#define CAN_RX_GPIO GPIO_NUM_10

void can_init()
{
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_GPIO, CAN_RX_GPIO, TWAI_MODE_NORMAL);
    g_config.intr_flags = ESP_INTR_FLAG_LEVEL1 | ESP_INTR_FLAG_SHARED;

    twai_timing_config_t  t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t  f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    esp_err_t err = twai_driver_install(&g_config, &t_config, &f_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "twai_driver_install failed: %s", esp_err_to_name(err));
        return;
    }

    err = twai_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "twai_start failed: %s", esp_err_to_name(err));
        twai_driver_uninstall();
        return;
    }

    ESP_LOGI(TAG, "TWAI ready at 500 kbit/s (TX=GPIO%d RX=GPIO%d)", CAN_TX_GPIO, CAN_RX_GPIO);
}
