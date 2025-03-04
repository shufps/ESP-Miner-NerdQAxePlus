#pragma once

#include "asic.h"
#include "bm1366.h"
#include "board.h"

class NerdAxe : public Board {
  protected:
    uint8_t ds4432_tps40305_bitaxe_voltage_to_reg(float vout);

  public:
    NerdAxe();

    virtual void shutdown();

    virtual bool initBoard();
    virtual bool initAsics();

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
    virtual bool selfTest();
};
