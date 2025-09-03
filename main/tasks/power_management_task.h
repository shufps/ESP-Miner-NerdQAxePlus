#pragma once

#include <pthread.h>
#include "boards/board.h"
#include "pid/PID_v1_bc.h"

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
    pthread_mutex_t m_mutex;
    uint16_t m_fanPerc[Board::FAN_COUNT];
    uint16_t m_fanRPM[Board::FAN_COUNT];
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
    uint16_t getFanRPM(int fan = 0)
    {
        return m_fanRPM[fan];
    };
    uint16_t getFanPerc(int fan = 0)
    {
        return m_fanPerc[fan];
    };

    void lock() {
        pthread_mutex_lock(&m_mutex);
    }

    void unlock() {
        pthread_mutex_unlock(&m_mutex);
    }
};
