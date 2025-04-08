#include "pid_timer.h"
#include <algorithm>
#include "esp_log.h"

static const char* TAG = "PidTimer";

PidTimer::PidTimer() {

    m_input = 0.0f;
    m_output = 0.0f;
    m_setpoint = 0.0f;
    m_filteredTemp = 0.0f;
    m_temp = 0.0f;
    m_timer = nullptr;
    m_running = false;

    // we set the tunings explicitely later
    m_pid = new PID(&m_input, &m_output, &m_setpoint, DIRECT);

    m_pid->setSampleTime(100);
    m_pid->setOutputLimits(35, 100);
    m_pid->setMode(AUTOMATIC);
    m_pid->setControllerDirection(REVERSE);
    m_pid->initialize();
}

PidTimer::~PidTimer() {
    stop();
    delete m_pid;
}

void PidTimer::start() {
    esp_timer_create_args_t timerArgs = {
        .callback = &PidTimer::timerCallbackWrapper,
        .arg = this,
        .name = "pid_timer"
    };

    ESP_ERROR_CHECK(esp_timer_create(&timerArgs, &m_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(m_timer, 100000));

    m_running = true;
    ESP_LOGI(TAG, "PID timer started");
}

void PidTimer::stop() {
    if (m_running && m_timer != nullptr) {
        esp_timer_stop(m_timer);
        esp_timer_delete(m_timer);
        m_timer = nullptr;
        m_running = false;
        ESP_LOGI(TAG, "PID timer stopped");
    }

    delete m_pid;
    m_pid = nullptr;
}

void PidTimer::setTemp(float temp) {
    m_temp = temp;
}

float PidTimer::getFilteredInput() const {
    return m_filteredTemp;
}

float PidTimer::getTarget() const {
    return m_setpoint;
}

float PidTimer::getOutput() const {
    return m_output;
}

void PidTimer::setTarget(float target) {
    m_setpoint = target;
}

void PidTimer::setTunings(float p, float i, float d) {
    if (m_pid) {
        m_pid->setTunings(p, i, d);
    }
}

float PidTimer::getKp() const {
    return m_pid ? m_pid->getKp() : 0.0f;
}

float PidTimer::getKi() const {
    return m_pid ? m_pid->getKi() : 0.0f;
}

float PidTimer::getKd() const {
    return m_pid ? m_pid->getKd() : 0.0f;
}

void PidTimer::timerCallbackWrapper(void* arg) {
    static_cast<PidTimer*>(arg)->timerCallback();
}

void PidTimer::timerCallback() {
    constexpr float alpha = 0.05f;
    m_filteredTemp = alpha * m_temp + (1.0f - alpha) * m_filteredTemp;
    m_input = m_filteredTemp;

    if (m_pid) {
        m_pid->compute();
    }
}
