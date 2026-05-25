#include "can_task.h"

#include "driver/twai.h"
#include "esp_log.h"

static const char *TAG = "can";

void can_init(int tx_gpio, int rx_gpio)
{
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
        (gpio_num_t) tx_gpio, (gpio_num_t) rx_gpio, TWAI_MODE_NORMAL);
    g_config.rx_queue_len = 32;  // default=5 is too small for 7-frame telemetry bursts
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

    ESP_LOGI(TAG, "TWAI ready at 500 kbit/s (TX=GPIO%d RX=GPIO%d)", tx_gpio, rx_gpio);
}
