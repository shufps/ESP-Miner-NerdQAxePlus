#pragma once

#include "asic.h"
#include "bm1368.h"
#include "board.h"

class NerdQaxePlus : public Board {
  protected:
    int m_numPhases;
    int m_imax;
    float m_ifault;
    int m_initVoltageMillis;

    void LDO_enable();
    void LDO_disable();

    int detectNumTempSensors();

  public:
    NerdQaxePlus();

    virtual bool initBoard();
    virtual bool initAsics();

    virtual void shutdown();

// abstract common methos
    virtual bool setVoltage(float core_voltage);

    virtual void setFanSpeed(float perc);
    virtual void getFanSpeed(uint16_t *rpm);

    virtual float getTemperature(int index);
    virtual float getVRTemp();

    virtual float getVin();
    virtual float getIin();
    virtual float getPin();
    virtual float getVout();
    virtual float getIout();
    virtual float getPout();
    virtual void requestBuckTelemtry();

    virtual bool getPSUFault();
    virtual bool selfTest();
};
