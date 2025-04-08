#ifndef _ESP_PID_V1_BC_H_
#define _ESP_PID_V1_BC_H_

#include <stdint.h>

#define LIBRARY_VERSION 1.2.6

#define AUTOMATIC   1
#define MANUAL      0
#define DIRECT      0
#define REVERSE     1
#define P_ON_M      0
#define P_ON_E      1

class PID {
public:
    PID(float* input, float* output, float* setpoint, float kp, float ki, float kd, int pOn, int direction);
    PID(float* input, float* output, float* setpoint, float kp, float ki, float kd, int direction);
    PID(float* input, float* output, float* setpoint, int direction);

    bool compute();
    void setMode(int mode);
    void setOutputLimits(float min, float max);

    void setTunings(float kp, float ki, float kd);
    void setTunings(float kp, float ki, float kd, int pOn);
    void setTarget(float value);
    void setControllerDirection(int direction);
    void setSampleTime(int newSampleTime);

    void initialize();
    float m_outputSum;

    // Getters
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

    float* m_input;
    float* m_output;
    float* m_setpoint;

    int64_t m_lastTime;
    int64_t m_sampleTime;
    float m_outMin, m_outMax;
    float m_lastInput;
    bool m_inAuto;
};

#endif // _ESP_PID_V1_BC_H_
