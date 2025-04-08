#include "PID_v1_bc.h"
#include "esp_timer.h"

PID::PID()
{
    m_input = 0.0f;
    m_setpoint = 0.0;
    m_output = 0.0f;
    m_inAuto = false;
    m_sampleTime = 100;
}

void PID::init(float kp, float ki, float kd, int controllerDirection) {
    setOutputLimits(0, 255);
    setControllerDirection(controllerDirection);
    setTunings(kp, ki, kd, P_ON_E);
}

bool PID::compute()
{
    if (!m_inAuto) return false;

    float input = m_input;
    float error = m_setpoint - input;
    float dInput = input - m_lastInput;

    m_outputSum += (m_ki * error);

    if (!m_pOnE)
        m_outputSum -= m_kp * dInput;

    if (m_outputSum > m_outMax) m_outputSum = m_outMax;
    else if (m_outputSum < m_outMin) m_outputSum = m_outMin;

    float output;
    if (m_pOnE)
        output = m_kp * error;
    else
        output = 0;

    output += m_outputSum - m_kd * dInput;

    if (output > m_outMax) {
        m_outputSum -= output - m_outMax;
        output = m_outMax;
    } else if (output < m_outMin) {
        m_outputSum += m_outMin - output;
        output = m_outMin;
    }

    m_output = output;

    m_lastInput = input;
    return true;
}

void PID::setTunings(float kp, float ki, float kd, int pOn)
{
    if (kp < 0 || ki < 0 || kd < 0) return;

    m_pOn = pOn;
    m_pOnE = (pOn == P_ON_E);

    m_dispKp = kp;
    m_dispKi = ki;
    m_dispKd = kd;

    float sampleTimeInSec = ((float)m_sampleTime) / 1000;
    m_kp = kp;
    m_ki = ki * sampleTimeInSec;
    m_kd = kd / sampleTimeInSec;

    if (m_controllerDirection == REVERSE)
    {
        m_kp = -m_kp;
        m_ki = -m_ki;
        m_kd = -m_kd;
    }
}

void PID::setInput(float input) {
    m_input = input;
}

float PID::getOutput() {
    return m_output;
}

void PID::setTarget(float value) {
    m_setpoint = value;
}

float PID::getTarget() {
    return m_setpoint;
}

void PID::setTunings(float kp, float ki, float kd)
{
    setTunings(kp, ki, kd, m_pOn);
}

void PID::setSampleTime(int newSampleTime)
{
    if (newSampleTime > 0)
    {
        float ratio = (float)newSampleTime / (float)m_sampleTime;
        m_ki *= ratio;
        m_kd /= ratio;
        m_sampleTime = newSampleTime;
    }
}

void PID::setOutputLimits(float min, float max)
{
    if (min >= max) return;
    m_outMin = min;
    m_outMax = max;

    if (m_inAuto)
    {
        if (m_output > m_outMax) m_output = m_outMax;
        else if (m_output < m_outMin) m_output = m_outMin;

        if (m_outputSum > m_outMax) m_outputSum = m_outMax;
        else if (m_outputSum < m_outMin) m_outputSum = m_outMin;
    }
}

void PID::setMode(int mode)
{
    bool newAuto = (mode == AUTOMATIC);
    if (newAuto && !m_inAuto)
    {
        initialize();
    }
    m_inAuto = newAuto;
}

void PID::initialize()
{
    m_outputSum = m_output;
    m_lastInput = m_input;

    if (m_outputSum > m_outMax) m_outputSum = m_outMax;
    else if (m_outputSum < m_outMin) m_outputSum = m_outMin;
}

void PID::setControllerDirection(int direction)
{
    if (m_inAuto && direction != m_controllerDirection)
    {
        m_kp = -m_kp;
        m_ki = -m_ki;
        m_kd = -m_kd;
    }
    m_controllerDirection = direction;
}

float PID::getKp() { return m_dispKp; }
float PID::getKi() { return m_dispKi; }
float PID::getTi() { return m_dispKp / m_dispKi; }
float PID::getKd() { return m_dispKd; }
float PID::getTd() { return m_dispKd / m_dispKp; }
int PID::getMode() { return m_inAuto ? AUTOMATIC : MANUAL; }
int PID::getDirection() { return m_controllerDirection; }