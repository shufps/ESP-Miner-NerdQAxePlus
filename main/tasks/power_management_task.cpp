#include <math.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "mining.h"

#include "serial.h"
#include "global_state.h"
#include "nvs_config.h"
#include "influx_task.h"
#include "boards/board.h"

#define POLL_RATE 2000
#define THROTTLE_TEMP 65.0

static const char *TAG = "power_management";

PowerManagementTask::PowerManagementTask() {
    // NOP
}

// Set the fan speed between 20% min and 100% max based on chip temperature as input.
// The fan speed increases from 20% to 100% proportionally to the temperature increase from 50 and THROTTLE_TEMP
double PowerManagementTask::automaticFanSpeed(Board* board, float chip_temp)
{
    double result = 0.0;
    double min_temp = 45.0;
    double min_fan_speed = 35.0;

    if (chip_temp < min_temp) {
        result = min_fan_speed;
    } else if (chip_temp >= THROTTLE_TEMP) {
        result = 100;
    } else {
        double temp_range = THROTTLE_TEMP - min_temp;
        double fan_range = 100 - min_fan_speed;
        result = ((chip_temp - min_temp) / temp_range) * fan_range + min_fan_speed;
    }

    float perc = (float) result / 100;
    m_fanPerc = perc;
    board->setFanSpeed(perc);
    return result;
}

void PowerManagementTask::taskWrapper(void *pvParameters) {
    PowerManagementTask* powerManagementTask = (PowerManagementTask*) pvParameters;
    powerManagementTask->task();
}

void PowerManagementTask::task()
{
    Board* board = SYSTEM_MODULE.getBoard();


    uint16_t auto_fan_speed = nvs_config_get_u16(NVS_CONFIG_AUTO_FAN_SPEED, 1);

    vTaskDelay(3000 / portTICK_PERIOD_MS);

    uint16_t last_core_voltage = 0.0;
    uint16_t last_asic_frequency = 0;
    uint64_t last_temp_request = esp_timer_get_time();
    while (1) {
        // the asics are initialized after this task starts
        Asic* asics = board->getAsics();

        uint16_t core_voltage = nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE, CONFIG_ASIC_VOLTAGE);
        uint16_t asic_frequency = nvs_config_get_u16(NVS_CONFIG_ASIC_FREQ, CONFIG_ASIC_FREQUENCY);
        uint16_t overheat_temp = nvs_config_get_u16(NVS_CONFIG_OVERHEAT_TEMP, OVERHEAT_DEFAULT);

        if (core_voltage != last_core_voltage) {
            ESP_LOGI(TAG, "setting new vcore voltage to %umV", core_voltage);
            board->setVoltage((double) core_voltage / 1000.0);
            last_core_voltage = core_voltage;
        }

        if (asic_frequency != last_asic_frequency) {
            ESP_LOGI(TAG, "setting new asic frequency to %uMHz", asic_frequency);
            if (asics && !asics->setAsicFrequency((float) asic_frequency)) {
                ESP_LOGE(TAG, "pll setting not found for %uMHz", asic_frequency);
            }
            last_asic_frequency = asic_frequency;
        }

        // request chip temps and buck telemetry all 15s
        if (esp_timer_get_time() - last_temp_request > 15000000llu) {
            if (asics) {
                asics->requestChipTemp();
            }
            board->requestBuckTelemtry();
            last_temp_request = esp_timer_get_time();
        }

        float vin = board->getVin();
        float iin = board->getIin();
        float pin = board->getPin();
        float pout = board->getPout();
        float vout = board->getVout();
        float iout = board->getIout();

        influx_task_set_pwr(vin, iin, pin, vout, iout, pout);

        m_voltage = vin * 1000.0;
        m_current = iin * 1000.0;
        m_power = pin;
        board->getFanSpeed(&m_fanRPM);

        m_chipTempAvg = board->readTemperature(0);
        m_vrTemp = board->readTemperature(1);
        ESP_LOGI(TAG, "asic high temp: %.3f", m_asic_high_temp);

        // to show the high temp in dashboard
        if (m_asic_high_temp > m_chipTempAvg) m_chipTempAvg = m_asic_high_temp;

        influx_task_set_temperature(m_chipTempAvg, m_vrTemp);

        if (overheat_temp &&
            // To keep the sensor and asic temp separated, comment the if above and uncomment below
            // (m_chipTempAvg > overheat_temp || m_vrTemp > overheat_temp || m_asic_high_temp > overheat_temp)) {
            (m_chipTempAvg > overheat_temp || m_vrTemp > overheat_temp )) {
            // over temperature
            SYSTEM_MODULE.setOverheated(true);
            // disables the buck
            board->setVoltage(0.0);
        }

        if (auto_fan_speed == 1) {
            m_fanPerc = (float) automaticFanSpeed(board, m_chipTempAvg);
        } else {
            float fs = (float) nvs_config_get_u16(NVS_CONFIG_FAN_SPEED, 100);
            m_fanPerc = fs;
            board->setFanSpeed((float) fs / 100);
        }

        vTaskDelay(POLL_RATE / portTICK_PERIOD_MS);
    }
}
