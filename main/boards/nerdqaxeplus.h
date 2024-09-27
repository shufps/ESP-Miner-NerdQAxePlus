#pragma once

#include "asic.h"
#include "bm1368.h"
#include "board.h"

class NerdQaxePlus : public Board {
  protected:
    int m_numPhases;

    void LDO_enable();
    void LDO_disable();

    BM1368 asics;

  public:
    NerdQaxePlus();

    virtual bool init();

// abstract common methos
    virtual bool setVoltage(float core_voltage);
    virtual uint16_t getVoltageMv();

    virtual void setFanSpeed(float perc);
    virtual void getFanSpeed(uint16_t *rpm);

    virtual float readTemperature(int index);

    virtual float getVin();
    virtual float getIin();
    virtual float getPin();
    virtual float getVout();
    virtual float getIout();
    virtual float getPout();

    virtual Asic* getAsics() { return &asics; }
};