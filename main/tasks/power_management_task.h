#pragma once

#include <pthread.h>
#include "boards/board.h"
#include "pid/PID_v1_bc.h"

class PowerManagementTask {
  protected:
    pthread_mutex_t m_mutex;
    uint16_t m_fanPerc;
    uint16_t m_fanRPM;
    float m_chipTempMax;
    float m_vrTemp;
    float m_voltage;
    float m_power;
    float m_current;
    PID *m_pid;

    void requestChipTemps();
    void checkCoreVoltageChanged();
    void checkAsicFrequencyChanged();
    void checkPidSettingsChanged();
    void task();

  public:
    PowerManagementTask();

    // synchronized rebooting to now mess up i2c comms
    void restart();

    static void taskWrapper(void *pvParameters);

    float getPower()
    {
        return m_power;
    };
    float getVoltage()
    {
        return m_voltage;
    };
    float getCurrent()
    {
        return m_current;
    };
    float getChipTempMax()
    {
        return m_chipTempMax;
    };
    float getVRTemp()
    {
        return m_vrTemp;
    };
    uint16_t getFanRPM()
    {
        return m_fanRPM;
    };
    uint16_t getFanPerc()
    {
        return m_fanPerc;
    };

    void lock() {
        pthread_mutex_lock(&m_mutex);
    }

    void unlock() {
        pthread_mutex_unlock(&m_mutex);
    }
};
