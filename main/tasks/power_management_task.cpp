#include <math.h>
#include <string.h>
#include <algorithm>

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

static const char *TAG = "power_management";

PowerManagementTask::PowerManagementTask() {
    m_mutex = PTHREAD_MUTEX_INITIALIZER;
}

void PowerManagementTask::taskWrapper(void *pvParameters) {
    PowerManagementTask* powerManagementTask = (PowerManagementTask*) pvParameters;
    powerManagementTask->task();
}

void PowerManagementTask::restart() {
    ESP_LOGW(TAG, "Shutdown requested ...");
    // stops the main task
    pthread_mutex_lock(&m_mutex);

    ESP_LOGW(TAG, "HW lock acquired!");
    // shutdown asics and LDOs before reset
    Board* board = SYSTEM_MODULE.getBoard();
    board->shutdown();

    ESP_LOGW(TAG, "restart");
    esp_restart();
    pthread_mutex_unlock(&m_mutex);
}

void PowerManagementTask::task()
{
    Board* board = SYSTEM_MODULE.getBoard();

    vTaskDelay(3000 / portTICK_PERIOD_MS);

    uint16_t last_core_voltage = 0.0;
    uint16_t last_asic_frequency = 0;
    uint64_t last_temp_request = esp_timer_get_time();
    while (1) {
        pthread_mutex_lock(&m_mutex);
        // the asics are initialized after this task starts
        Asic* asics = board->getAsics();

        uint16_t core_voltage = board->getAsicVoltageMillis();
        uint16_t asic_frequency = board->getAsicFrequency();
        uint16_t asic_overheat_temp = Config::getOverheatTemp();
        bool auto_fan_speed = Config::isAutoFanSpeedEnabled();

        // overwrite previously allowed 0 value to disable
        // over-temp shutdown
        if (!asic_overheat_temp) {
            asic_overheat_temp = 70;
        }

        if (core_voltage != last_core_voltage) {
            ESP_LOGI(TAG, "setting new vcore voltage to %umV", core_voltage);
            board->setVoltage((float) core_voltage / 1000.0);
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

        m_vrTemp = board->getVRTemp();

        ESP_LOGI(TAG, "vin: %.2f, iin: %.2f, pin: %.2f, vout: %.2f, iout: %.2f, pout: %.2f, vr-temp: %.2f",
            vin, iin, pin, vout, iout, pout, m_vrTemp);

        influx_task_set_pwr(vin, iin, pin, vout, iout, pout);

        // currently only implemented for boards with TPS536x7
        bool psuError = board->getPSUFault();
        if (psuError) {
            // when this happens, there is some PSU error
            // on the nerdqaxes the buck restarted and defaulted to 1.00V
            // this can happen when the PSU can't deliver enough current
            // we display the error message and switch the buck off
            SYSTEM_MODULE.setPSUError(true);
            board->setVoltage(0.0);
        }

        m_voltage = vin * 1000.0;
        m_current = iin * 1000.0;
        m_power = pin;
        board->getFanSpeed(&m_fanRPM);

        // collect temperatures
        // get the max of all asic measuring temp sensors
        float tmp1075Max = 0.0f;
        for (int i=0; i < board->getNumTempSensors(); i++) {
            float tmp = board->getTemperature(i);
            if (tmp) {
                ESP_LOGI(TAG, "Temperature %d: %.2f C", i, tmp);
            }
            tmp1075Max = std::max(tmp1075Max, tmp);
        }

        //Get the readed MaxChipTemp of all chained chips after
        //calling requestChipTemp()
        //IMPORTANT: this value only makes sense with BM1368 ASIC, with other Asics will remain at 0
        float intChipTempMax = asics ? asics->getMaxChipTemp() : 0.0f;

        // Uses the worst case between board temp sensor or Asic temp read command
        m_chipTempMax = std::max(tmp1075Max, intChipTempMax);

        influx_task_set_temperature(m_chipTempMax, m_vrTemp);

        float vr_maxTemp = asic_overheat_temp;
        if(board->getVrMaxTemp()) {
            vr_maxTemp = board->getVrMaxTemp();
        }

        if (asic_overheat_temp &&
            (m_chipTempMax > asic_overheat_temp || m_vrTemp > vr_maxTemp)) {
            // over temperature
            SYSTEM_MODULE.setOverheated(true);
            // disables the buck
            board->setVoltage(0.0);
            ESP_LOGE(TAG, "System overheated - Shutting down asic voltage");
        }

        if (auto_fan_speed) {
            m_fanPerc = board->automaticFanSpeed(m_chipTempMax);
        } else {
            m_fanPerc = (float) Config::getFanSpeed();
            board->setFanSpeed(m_fanPerc / 100.0f);
        }
        pthread_mutex_unlock(&m_mutex);

        vTaskDelay(POLL_RATE / portTICK_PERIOD_MS);
    }
}
