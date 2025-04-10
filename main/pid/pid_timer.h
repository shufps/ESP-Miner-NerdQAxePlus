#pragma once

#include "PID_v1_bc.h"
#include "esp_timer.h"

class PidTimer : public PID {
  public:

    PidTimer(float alpha);
    ~PidTimer();

    void init(int sampletime, float kp, float ki, float kd, int controllerDirection) override;

    void start();
    void stop();

    void setInput(float input) override;
    float getFilteredInput();

  private:
    static void timerCallbackWrapper(void *arg);
    void timerCallback();

    float m_input;
    float m_filteredInput;

    float m_alpha;

    esp_timer_handle_t m_timer;
    bool m_running;
};
