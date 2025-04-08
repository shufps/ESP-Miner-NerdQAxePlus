#include "PID_v1_bc.h"
#include "esp_timer.h"

#define getMillis() (esp_timer_get_time() / 1000ULL)

PID::PID(float* input, float* output, float* setpoint,
         float kp, float ki, float kd, int pOn, int direction)
{
    m_input = input;
    m_output = output;
    m_setpoint = setpoint;
    m_inAuto = false;

    setOutputLimits(0, 255);
    m_sampleTime = 100;

    setControllerDirection(direction);
    setTunings(kp, ki, kd, pOn);

    m_lastTime = getMillis() - m_sampleTime;
}

PID::PID(float* input, float* output, float* setpoint,
         float kp, float ki, float kd, int direction)
    : PID(input, output, setpoint, kp, ki, kd, P_ON_E, direction) {}

PID::PID(float* input, float* output, float* setpoint, int direction)
    : PID(input, output, setpoint, -1, -1, -1, P_ON_E, direction) {}

bool PID::compute() {
    if (!m_inAuto) return false;

    unsigned long now = getMillis();
    unsigned long timeChange = now - m_lastTime;

    // assuming compute is called at fixed intervals
    {
        float input = *m_input;
        float error = *m_setpoint - input;
        float dInput = input - m_lastInput;

        m_outputSum += (m_ki * error);

        if (!m_pOnE)
            m_outputSum -= m_kp * dInput;

        // Clamp output sum
        if (m_outputSum > m_outMax) m_outputSum = m_outMax;
        else if (m_outputSum < m_outMin) m_outputSum = m_outMin;

        float output = m_pOnE ? m_kp * error : 0.0f;
        output += m_outputSum - m_kd * dInput;

        // Clamp output and adjust integral to prevent wind-up
        if (output > m_outMax) {
            m_outputSum -= output - m_outMax;
            output = m_outMax;
        } else if (output < m_outMin) {
            m_outputSum += m_outMin - output;
            output = m_outMin;
        }

        *m_output = output;

        m_lastInput = input;
        m_lastTime = now;
        return true;
    }

    return false;
}

void PID::setTunings(float kp, float ki, float kd, int pOn) {
    if (kp < 0 || ki < 0 || kd < 0) return;

    m_pOn = pOn;
    m_pOnE = (pOn == P_ON_E);

    m_dispKp = kp;
    m_dispKi = ki;
    m_dispKd = kd;

    float sampleTimeSec = ((float)m_sampleTime) / 1000.0f;
    m_kp = kp;
    m_ki = ki * sampleTimeSec;
    m_kd = kd / sampleTimeSec;

    if (m_controllerDirection == REVERSE) {
        m_kp = -m_kp;
        m_ki = -m_ki;
        m_kd = -m_kd;
    }
}

void PID::setTunings(float kp, float ki, float kd) {
    setTunings(kp, ki, kd, m_pOn);
}

void PID::setTarget(float value) {
    *m_setpoint = value;
}

float PID::getTarget() {
    return *m_setpoint;
}

void PID::setSampleTime(int newSampleTime) {
    if (newSampleTime > 0) {
        float ratio = (float)newSampleTime / (float)m_sampleTime;
        m_ki *= ratio;
        m_kd /= ratio;
        m_sampleTime = newSampleTime;
    }
}

void PID::setOutputLimits(float min, float max) {
    if (min >= max) return;
    m_outMin = min;
    m_outMax = max;

    if (m_inAuto) {
        if (*m_output > m_outMax) *m_output = m_outMax;
        else if (*m_output < m_outMin) *m_output = m_outMin;

        if (m_outputSum > m_outMax) m_outputSum = m_outMax;
        else if (m_outputSum < m_outMin) m_outputSum = m_outMin;
    }
}

void PID::setMode(int mode) {
    bool newAuto = (mode == AUTOMATIC);
    if (newAuto && !m_inAuto) {
        initialize();
    }
    m_inAuto = newAuto;
}

void PID::initialize() {
    m_outputSum = *m_output;
    m_lastInput = *m_input;

    if (m_outputSum > m_outMax) m_outputSum = m_outMax;
    else if (m_outputSum < m_outMin) m_outputSum = m_outMin;
}

void PID::setControllerDirection(int direction) {
    if (m_inAuto && direction != m_controllerDirection) {
        m_kp = -m_kp;
        m_ki = -m_ki;
        m_kd = -m_kd;
    }
    m_controllerDirection = direction;
}

float PID::getKp() { return m_dispKp; }
float PID::getKi() { return m_dispKi; }
float PID::getTi() { return m_dispKi != 0 ? m_dispKp / m_dispKi : 0.0f; }
float PID::getKd() { return m_dispKd; }
float PID::getTd() { return m_dispKp != 0 ? m_dispKd / m_dispKp : 0.0f; }
int PID::getMode() { return m_inAuto ? AUTOMATIC : MANUAL; }
int PID::getDirection() { return m_controllerDirection; }
