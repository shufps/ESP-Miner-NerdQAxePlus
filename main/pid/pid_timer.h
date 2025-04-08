#pragma once

#include "PID_v1_bc.h"
#include "esp_timer.h"

class PidTimer : public PID {
  public:

    PidTimer(int sampletime, float alpha);
    ~PidTimer();

    void init(float kp, float ki, float kd, int controllerDirection) override;

    void start();
    void stop();

    void setInput(float input) override;
    float getFilteredInput();

  private:
    static void timerCallbackWrapper(void *arg);
    void timerCallback();

    float m_input;
    float m_filteredInput;

    int m_sampleTime;
    float m_alpha;

    esp_timer_handle_t m_timer;
    bool m_running;
};
