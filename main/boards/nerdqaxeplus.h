#pragma once

#include "asic.h"
#include "bm1368.h"
#include "board.h"
#include "board-params.h"

class NerdQaxePlus : public Board {
  protected:
    BoardParameters m_params;
    int m_numPhases;
    int m_imax;
    float m_ifault;
    float m_initVoltage;

    void LDO_enable();
    void LDO_disable();

  public:
    NerdQaxePlus();

    virtual bool initBoard();
    virtual bool initAsics();

    virtual void shutdown();

// abstract common methos
    virtual bool setVoltage(float core_voltage);

    virtual void setFanSpeed(float perc);
    virtual void getFanSpeed(uint16_t *rpm);

    virtual float readTemperature(int index);

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