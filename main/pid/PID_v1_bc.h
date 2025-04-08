#ifndef _ESP_PID_V1_BC_H_
#define _ESP_PID_V1_BC_H_

#include <stdint.h>

#define LIBRARY_VERSION	1.2.6

#define AUTOMATIC	1
#define MANUAL	    0
#define DIRECT      0
#define REVERSE     1
#define P_ON_M      0
#define P_ON_E      1

// we need packed here (for memcmp)
struct __attribute__((packed)) PidSettings {
    uint16_t targetTemp;
    uint16_t p;
    uint16_t i;
    uint16_t d;
};


class PID
{
public:
    PID();

    virtual void init(float kp, float ki, float kd, int controllerDirection);

    void setMode(int mode);
    bool compute();
    void setOutputLimits(float, float);

    void setTunings(float, float, float);
    void setTunings(float, float, float, int);
    void setTarget(float value);
    void setControllerDirection(int);
    void setSampleTime(int);

    virtual void setInput(float input);
    float getOutput();

    void initialize();
    float m_outputSum;

    float getKp();
    float getKi();
    float getTi();
    float getKd();
    float getTd();
    int getMode();
    int getDirection();
    float getTarget();

private:
    float m_dispKp, m_dispKi, m_dispKd;
    float m_kp, m_ki, m_kd;
    int m_controllerDirection;
    int m_pOn;
    bool m_pOnE;

    float m_input;
    float m_output;
    float m_setpoint;

    int64_t m_sampleTime;
    float m_outMin, m_outMax;
    float m_lastInput;
    bool m_inAuto;
};

#endif // _ESP_PID_V1_BC_H_
