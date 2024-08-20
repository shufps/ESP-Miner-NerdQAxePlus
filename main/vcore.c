#include "esp_log.h"
#include <math.h>
#include <stdio.h>

#include "TPS53647.h"
#include "vcore.h"

static const char *TAG = "vcore.c";

void VCORE_init(GlobalState *global_state)
{
    switch (global_state->device_model) {
    case DEVICE_NERDQAXE_PLUS:
        TPS53647_init();
        break;
    default:
    }
}

bool VCORE_set_voltage(float core_voltage, GlobalState *global_state)
{
    switch (global_state->device_model) {
    case DEVICE_NERDQAXE_PLUS:
        ESP_LOGI(TAG, "Set ASIC voltage = %.3fV", core_voltage);
        TPS53647_set_vout(core_voltage * (float) global_state->voltage_domain);
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
