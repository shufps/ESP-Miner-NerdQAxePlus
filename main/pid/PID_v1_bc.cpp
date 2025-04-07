#include "PID_v1_bc.h"
#include "esp_timer.h"

#define getMillis() (esp_timer_get_time() / 1000ULL)

PID::PID(float* Input, float* Output, float* Setpoint,
         float Kp, float Ki, float Kd, int POn, int ControllerDirection)
{
    myOutput = Output;
    myInput = Input;
    mySetpoint = Setpoint;
    inAuto = false;

    SetOutputLimits(0, 255);
    SampleTime = 100;

    SetControllerDirection(ControllerDirection);
    SetTunings(Kp, Ki, Kd, POn);

    lastTime = getMillis() - SampleTime;
}

PID::PID(float* Input, float* Output, float* Setpoint,
         float Kp, float Ki, float Kd, int ControllerDirection)
    : PID(Input, Output, Setpoint, Kp, Ki, Kd, P_ON_E, ControllerDirection)
{
}

bool PID::Compute()
{
    if (!inAuto) return false;

    unsigned long now = getMillis();
    unsigned long timeChange = now - lastTime;

    // we make sure that we only call it in the correct sample interval
    if (true /*timeChange >= SampleTime*/)
    {
        float input = *myInput;
        float error = *mySetpoint - input;
        float dInput = input - lastInput;

        outputSum += (ki * error);

        if (!pOnE)
            outputSum -= kp * dInput;

        if (outputSum > outMax) outputSum = outMax;
        else if (outputSum < outMin) outputSum = outMin;

        float output;
        if (pOnE)
            output = kp * error;
        else
            output = 0;

        output += outputSum - kd * dInput;

        if (output > outMax) {
            outputSum -= output - outMax;
            output = outMax;
        } else if (output < outMin) {
            outputSum += outMin - output;
            output = outMin;
        }

        *myOutput = output;

        lastInput = input;
        lastTime = now;
        return true;
    }
    return false;
}

void PID::SetTunings(float Kp, float Ki, float Kd, int POn)
{
    if (Kp < 0 || Ki < 0 || Kd < 0) return;

    pOn = POn;
    pOnE = (POn == P_ON_E);

    dispKp = Kp;
    dispKi = Ki;
    dispKd = Kd;

    float SampleTimeInSec = ((float)SampleTime) / 1000;
    kp = Kp;
    ki = Ki * SampleTimeInSec;
    kd = Kd / SampleTimeInSec;

    if (controllerDirection == REVERSE)
    {
        kp = -kp;
        ki = -ki;
        kd = -kd;
    }
}

void PID::SetTarget(float value) {
    *mySetpoint = value;
}

float PID::GetTarget() {
    return *mySetpoint;
}

void PID::SetTunings(float Kp, float Ki, float Kd)
{
    SetTunings(Kp, Ki, Kd, pOn);
}

void PID::SetSampleTime(int NewSampleTime)
{
    if (NewSampleTime > 0)
    {
        float ratio = (float)NewSampleTime / (float)SampleTime;
        ki *= ratio;
        kd /= ratio;
        SampleTime = NewSampleTime;
    }
}

void PID::SetOutputLimits(float Min, float Max)
{
    if (Min >= Max) return;
    outMin = Min;
    outMax = Max;

    if (inAuto)
    {
        if (*myOutput > outMax) *myOutput = outMax;
        else if (*myOutput < outMin) *myOutput = outMin;

        if (outputSum > outMax) outputSum = outMax;
        else if (outputSum < outMin) outputSum = outMin;
    }
}

void PID::SetMode(int Mode)
{
    bool newAuto = (Mode == AUTOMATIC);
    if (newAuto && !inAuto)
    {
        Initialize();
    }
    inAuto = newAuto;
}

void PID::Initialize()
{
    outputSum = *myOutput;
    lastInput = *myInput;

    if (outputSum > outMax) outputSum = outMax;
    else if (outputSum < outMin) outputSum = outMin;
}

void PID::SetControllerDirection(int Direction)
{
    if (inAuto && Direction != controllerDirection)
    {
        kp = -kp;
        ki = -ki;
        kd = -kd;
    }
    controllerDirection = Direction;
}

float PID::GetKp() { return dispKp; }
float PID::GetKi() { return dispKi; }
float PID::GetTi() { return dispKp / dispKi; }
float PID::GetKd() { return dispKd; }
float PID::GetTd() { return dispKd / dispKp; }
int PID::GetMode() { return inAuto ? AUTOMATIC : MANUAL; }
int PID::GetDirection() { return controllerDirection; }
