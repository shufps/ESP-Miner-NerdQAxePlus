#include "pid_timer.h"
#include "esp_log.h"
#include <algorithm>

static const char *TAG = "PidTimer";

PidTimer::PidTimer(float alpha) : PID() {
    m_input = 0.0f;
    m_alpha = alpha;
    m_filteredInput = 0;
    m_timer = nullptr;
    m_running = false;

}

void PidTimer::init(int sampletime, float kp, float ki, float kd, int controllerDirection) {
    PID::init(sampletime, kp, ki, kd, controllerDirection);
}

PidTimer::~PidTimer()
{
    stop();
}

void PidTimer::setInput(float input) {
    m_input = input;
}

float PidTimer::getFilteredInput()
{
    return m_filteredInput;
}

void PidTimer::timerCallbackWrapper(void *arg)
{
    static_cast<PidTimer *>(arg)->timerCallback();
}

void PidTimer::timerCallback()
{
    m_filteredInput = m_alpha * m_input + (1.0f - m_alpha) * m_filteredInput;

    PID::setInput(m_filteredInput);
    compute();
}

void PidTimer::start()
{
    esp_timer_create_args_t timerArgs = {.callback = &PidTimer::timerCallbackWrapper, .arg = this, .name = "pid_timer"};

    ESP_ERROR_CHECK(esp_timer_create(&timerArgs, &m_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(m_timer, getSampleTime() * 1000));

    m_running = true;
    ESP_LOGI(TAG, "PID timer started");
}

void PidTimer::stop()
{
    if (m_running && m_timer != nullptr) {
        esp_timer_stop(m_timer);
        esp_timer_delete(m_timer);
        m_timer = nullptr;
        m_running = false;
        ESP_LOGI(TAG, "PID timer stopped");
    }
}


