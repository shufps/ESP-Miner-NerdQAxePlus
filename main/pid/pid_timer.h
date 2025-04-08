#pragma once

#include "PID_v1_bc.h"
#include "esp_timer.h"

class PidTimer {
public:
    PidTimer();
    ~PidTimer();

    void start();
    void stop();

    void setTemp(float temp);

    void setTunings(float p, float i, float d);
    void setTarget(float target);

    float getTarget() const;
    float getOutput() const;
    float getFilteredInput() const;

    float getKp() const;
    float getKi() const;
    float getKd() const;

private:
    static void timerCallbackWrapper(void* arg);
    void timerCallback();

    float m_input;
    float m_output;
    float m_setpoint;
    float m_filteredTemp;
    float m_temp;

    PID* m_pid;
    esp_timer_handle_t m_timer;
    bool m_running;
};
