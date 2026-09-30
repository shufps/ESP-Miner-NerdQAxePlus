#pragma once

#include "asic.h"
#include "bm1373.h"
#include "board.h"
#include "nerdaxe.h"

class NerdaxeGaia : public NerdAxe {
  protected:
    int m_initVoltageMillis;

    // LDO enable line (GPIO12) — power sequencing helpers
    void LDO_enable();
    void LDO_disable();

  public:
    NerdaxeGaia();

    virtual bool initBoard();
    virtual bool initAsics();

    virtual void shutdown();

    virtual bool setVoltage(float core_voltage);

    virtual float getTemperature(int index);
    virtual float getVRTemp();
    virtual bool isPIDAvailable() { return true; }

    virtual float getVin();
    virtual float getIin();
    virtual float getPin();
    virtual float getVout();
    virtual float getIout();
    virtual float getPout();
};
