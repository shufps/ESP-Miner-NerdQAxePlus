#pragma once

#include "asic.h"
#include "bm1370.h"
#include "board.h"
#include "nerdaxe.h"

class NerdaxeGamma : public NerdAxe {
  protected:
    float m_initVoltage;

  public:
    NerdaxeGamma();

    virtual bool initBoard();
    virtual bool initAsics();

    virtual void shutdown();

// abstract common methos
    virtual bool setVoltage(float core_voltage);

    virtual float readTemperature(int index);

    virtual float getVin();
    virtual float getIin();
    virtual float getPin();
    virtual float getVout();
    virtual float getIout();
    virtual float getPout();
};
