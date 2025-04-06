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

class PID
{
public:
    PID(float*, float*, float*, float, float, float, int, int);
    PID(float*, float*, float*, float, float, float, int);

    void SetMode(int Mode);
    bool Compute();
    void SetOutputLimits(float, float);

    void SetTunings(float, float, float);
    void SetTunings(float, float, float, int);
    void SetTarget(float value);
    void SetControllerDirection(int);
    void SetSampleTime(int);

    void Initialize();       // bumpless transfer
    float outputSum;        // allow user to inspect/influence integral state

    // Display functions
    float GetKp();
    float GetKi();
    float GetTi();
    float GetKd();
    float GetTd();
    int GetMode();
    int GetDirection();

private:
    float dispKp, dispKi, dispKd;
    float kp, ki, kd;
    int controllerDirection;
    int pOn;
    bool pOnE;

    float *myInput;
    float *myOutput;
    float *mySetpoint;

    int64_t lastTime;
    int64_t SampleTime;
    float outMin, outMax;
    float lastInput;
    bool inAuto;
};

#endif // _ESP_PID_V1_BC_H_
