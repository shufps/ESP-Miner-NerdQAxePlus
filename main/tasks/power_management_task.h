#pragma once

#include <pthread.h>
#include "boards/board.h"
#include "pid/PID_v1_bc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_timer.h"


template <class T>
class LockGuard {
public:
    LockGuard(T& obj) : m_obj(obj) { m_obj.lock(); }
    ~LockGuard() { m_obj.unlock(); }
private:
    T& m_obj;
};

class PowerManagementTask {
  protected:
    pthread_mutex_t m_loop_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t m_loop_cond = PTHREAD_COND_INITIALIZER;

    SemaphoreHandle_t m_mutex;
    TimerHandle_t m_timer;

    char m_logBuffer[256] = {0};
    uint16_t m_fanPerc;
    uint16_t m_fanRPM;
    float m_chipTempMax;
    float m_vrTemp;
    float m_voltage;
    float m_power;
    float m_current;
    bool m_shutdown = false;
    PID *m_pid;

    void checkCoreVoltageChanged();
    void checkAsicFrequencyChanged();
    void checkPidSettingsChanged();
    void checkVrFrequencyChanged();
    void task();

    bool startTimer();
    void trigger();

    void logChipTemps();

  public:
    PowerManagementTask();

    // synchronized rebooting to now mess up i2c comms
    void restart();

    static void taskWrapper(void *pvParameters);
    static void create_job_timer(TimerHandle_t xTimer);

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
        xSemaphoreTakeRecursive(m_mutex, portMAX_DELAY);
    }

    void unlock() {
        xSemaphoreGiveRecursive(m_mutex);
    }

    void shutdown();
};
