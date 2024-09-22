#include "esp_log.h"
#include <math.h>
#include <stdio.h>

#include "boards/nerdqaxeplus.h"
#include "TPS53647.h"
#include "vcore.h"
#include "rom/gpio.h"
#include "driver/gpio.h"


static const char *TAG = "vcore.c";

void VCORE_init(float volts, GlobalState *global_state) {
    // configure gpios
    gpio_pad_select_gpio(TPS53647_EN_PIN);
    gpio_pad_select_gpio(LDO_EN_PIN);
    gpio_pad_select_gpio(BM1368_RST_PIN);

    gpio_set_direction(TPS53647_EN_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(LDO_EN_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(BM1368_RST_PIN, GPIO_MODE_OUTPUT);

    // disable buck (disabled EN pin)
    VCORE_set_voltage(0.0, global_state);

    // disable LDO
    VCORE_LDO_disable(global_state);

    // set reset high
    gpio_set_level(BM1368_RST_PIN, 1);

    // wait 250ms
    vTaskDelay(250 / portTICK_PERIOD_MS);

    // enable LDOs
    VCORE_LDO_enable(global_state);

    // wait 100ms
    vTaskDelay(100 / portTICK_PERIOD_MS);

    // init buck and enable output
    TPS53647_init();
    VCORE_set_voltage(volts / 1000.0, global_state);

    // wait 250ms
    vTaskDelay(250 / portTICK_PERIOD_MS);

    // release reset
    //gpio_set_level(BM1368_RST_PIN, 1);
}

void VCORE_LDO_enable(GlobalState *global_state) {
    switch (global_state->device_model) {
    case DEVICE_NERDQAXE_PLUS:
        ESP_LOGI(TAG, "Enabled LDOs");
        gpio_set_level(LDO_EN_PIN, 1);
        break;
    default:
    }
}

void VCORE_LDO_disable(GlobalState *global_state) {
    switch (global_state->device_model) {
    case DEVICE_NERDQAXE_PLUS:
        ESP_LOGI(TAG, "Disable LDOs");
        gpio_set_level(LDO_EN_PIN, 0);
        break;
    default:
    }
}


bool VCORE_set_voltage(float core_voltage, GlobalState *global_state)
{
    switch (global_state->device_model) {
    case DEVICE_NERDQAXE_PLUS:
        ESP_LOGI(TAG, "Set ASIC voltage = %.3fV", core_voltage);
        TPS53647_set_vout(core_voltage);
        break;
    default:
    }

    return true;
}

uint16_t VCORE_get_voltage_mv(GlobalState *global_state)
{
    switch (global_state->device_model) {
    case DEVICE_NERDQAXE_PLUS:
        return TPS53647_get_vout() * 1000.0f;
    default:
    }
    return 0;
}
