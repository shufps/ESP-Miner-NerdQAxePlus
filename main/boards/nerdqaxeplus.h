#pragma once

#include "asic.h"
#include "bm1368.h"
#include "board.h"
#include "./drivers/TPS53647.h"

class NerdQaxePlus : public Board {
  protected:
    int m_numPhases;
    int m_imax;
    float m_ifault;
    int m_initVoltageMillis;

    void LDO_enable();
    void LDO_disable();

    int detectNumTempSensors();

    TPS53647 *m_tps;

  public:
    NerdQaxePlus();

    virtual bool initBoard();
    virtual bool initAsics();

    virtual void shutdown();

    virtual bool setVoltage(float core_voltage);

    virtual void setFanPolarity(bool invert);
    virtual void setFanSpeedCh(int channel, float perc);
    virtual void getFanSpeedCh(int channel, uint16_t *rpm);

    virtual float getTemperature(int index);
    virtual float getVRTemp();
    virtual bool isPIDAvailable() { return true; }

    virtual float getVin();
    virtual float getIin();
    virtual float getPin();
    virtual float getVout();
    virtual float getIout();
    virtual float getPout();
    virtual void requestBuckTelemtry();
    virtual void requestChipTemps();

    virtual bool getPSUFault();
    virtual bool selfTest();
};
