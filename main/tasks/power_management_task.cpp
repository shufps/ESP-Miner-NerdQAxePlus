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
    lock();

    ESP_LOGW(TAG, "HW lock acquired!");
    // shutdown asics and LDOs before reset
    Board* board = SYSTEM_MODULE.getBoard();
    board->shutdown();

    ESP_LOGW(TAG, "restart");
    esp_restart();
    unlock();
}

void PowerManagementTask::requestChipTemps() {
    static uint64_t last_temp_request = esp_timer_get_time();

    Board* board = SYSTEM_MODULE.getBoard();
    Asic* asics = board->getAsics();

    // request chip temps and buck telemetry all 15s
    if (esp_timer_get_time() - last_temp_request > 15000000llu) {
        if (asics) {
            asics->requestChipTemp();
        }
        board->requestBuckTelemtry();
        last_temp_request = esp_timer_get_time();
    }
}

void PowerManagementTask::checkCoreVoltageChanged() {
    static uint16_t last_core_voltage = 0;

    Board* board = SYSTEM_MODULE.getBoard();

    uint16_t core_voltage = board->getAsicVoltageMillis();

    if (core_voltage != last_core_voltage) {
        ESP_LOGI(TAG, "setting new vcore voltage to %umV", core_voltage);
        board->setVoltage((float) core_voltage / 1000.0);
        last_core_voltage = core_voltage;
    }
}

void PowerManagementTask::checkAsicFrequencyChanged() {
    static uint16_t last_asic_frequency = 0;

    Board* board = SYSTEM_MODULE.getBoard();
    Asic* asics = board->getAsics();

    uint16_t asic_frequency = board->getAsicFrequency();

    if (asic_frequency != last_asic_frequency) {
        ESP_LOGI(TAG, "setting new asic frequency to %uMHz", asic_frequency);
        if (asics && !asics->setAsicFrequency((float) asic_frequency)) {
            ESP_LOGE(TAG, "pll setting not found for %uMHz", asic_frequency);
        }
        last_asic_frequency = asic_frequency;
    }
}

void PowerManagementTask::checkPidSettingsChanged() {
    static PidSettings oldPidSettings = {0, 0, 0, 0};

    Board* board = SYSTEM_MODULE.getBoard();
    PidSettings *pidSettings = board->getPidSettings();

    // we can use memcmp because we have a packed struct
    if (memcmp(pidSettings, &oldPidSettings, sizeof(PidSettings)) != 0) {
        ESP_LOGI(TAG, "PID settings change detected");

        float pidP = (float) pidSettings->p / 100.0f;
        float pidI = (float) pidSettings->i / 100.0f;
        float pidD = (float) pidSettings->d / 100.0f;

        m_pid->SetTunings(pidP, pidI, pidD);
        m_pid->SetTarget((float) pidSettings->targetTemp);
        ESP_LOGI(TAG, "temp: %.2f p:%.2f i:%.2f d:%.2f", m_pid->GetTarget(), m_pid->GetKp(), m_pid->GetKi(), m_pid->GetKd());        oldPidSettings = *pidSettings;
    }
}

void PowerManagementTask::task()
{
    Board* board = SYSTEM_MODULE.getBoard();

    // use manual invert polarity setting
    bool invert = board->isInvertFanPolarityEnabled();

    // and overwrite it if auto detection is enabled and
    // test was conclusive
    if (board->isAutoFanPolarityEnabled()) {
        lock();
        switch (board->guessFanPolarity()) {
            case POLARITY_NORMAL:
                invert = false;
                break;
            case POLARITY_INVERTED:
                invert = true;
                break;
            case POLARITY_UNKNOWN:
                // nop, we couldn't detect it
                break;
        }
        unlock();
    }
    board->setFanPolarity(invert);

    // pointer to pid settings
    PidSettings *pidSettings = board->getPidSettings();

    float pid_input = 0.0;
    float pid_output = 0.0;
    float pid_target = (float) pidSettings->targetTemp;

    float pidP = (float) pidSettings->p / 100.0f;
    float pidI = (float) pidSettings->i / 100.0f;
    float pidD = (float) pidSettings->d / 100.0f;

    m_pid = new PID(&pid_input, &pid_output, &pid_target, pidP, pidI, pidD, P_ON_E, DIRECT);
    m_pid->SetSampleTime(POLL_RATE);
    m_pid->SetOutputLimits(15, 100);
    m_pid->SetMode(AUTOMATIC);
    m_pid->SetControllerDirection(REVERSE);
    m_pid->Initialize();

    vTaskDelay(pdMS_TO_TICKS(3000));

    while (1) {
        lock();
        // the asics are initialized after this task starts
        Asic* asics = board->getAsics();

        uint16_t asic_overheat_temp = Config::getOverheatTemp();
        uint16_t temp_control_mode = Config::getTempControlMode();

        // overwrite previously allowed 0 value to disable
        // over-temp shutdown
        if (!asic_overheat_temp) {
            asic_overheat_temp = 70;
        }

        // check if asic voltage changed
        checkCoreVoltageChanged();

        // check if asic frequency changed
        checkAsicFrequencyChanged();

        // check if pid settings changed
        checkPidSettingsChanged();

        // request chip temps
        requestChipTemps();

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
        float intChipTempMax = board->getMaxChipTemp();

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

        // we let the PID always calculate for "bumpless transfer"
        // when switching modes
        pid_input = std::max(m_chipTempMax, m_vrTemp);
        m_pid->Compute();

        switch (temp_control_mode) {
            case 1:
                // classic automatic fan control
                m_fanPerc = board->automaticFanSpeed(m_chipTempMax);
                board->setFanSpeed(m_fanPerc / 100.0f);
                break;
            case 2:
                // pid
                m_fanPerc = roundf(pid_output);
                board->setFanSpeed(m_fanPerc / 100.0f);
                //ESP_LOGI(TAG, "PID: Temp: %.1f°C, SetPoint: %.1f°C, Output: %.1f%%", pid_input, pid_target, pid_output);
                //ESP_LOGI(TAG, "p:%.2f i:%.2f d:%.2f", m_pid->GetKp(), m_pid->GetKi(), m_pid->GetKd());
                break;
            default:
                ESP_LOGE(TAG, "invalid temp control mode: %d. Defaulting to manual.", temp_control_mode);
                [[fallthrough]];
            case 0:
                // manual
                m_fanPerc = (float) Config::getFanSpeed();
                board->setFanSpeed(m_fanPerc / 100.0f);
                break;
        }
        unlock();

        vTaskDelay(pdMS_TO_TICKS(POLL_RATE));
    }
}
