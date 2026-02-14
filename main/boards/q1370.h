#pragma once

#include "asic.h"
#include "bm1370.h"
#include "board.h"
#include "nerdqaxeplus.h"
#include "./drivers/fxl6408.h"
#include "./drivers/tmp451_mux_exp.h"

class Q1370B : public NerdQaxePlus {
  protected:
    Tmp451MuxExp* m_tmp451 = nullptr;

    // flag to remember if we found the tmux
    bool m_hasTMux = false;

    Fxl6408 m_io;

    virtual void LDO_enable();
    virtual void LDO_disable();

    virtual void VREG_enable();
    virtual void VREG_disable();

    virtual void setAsicReset(bool state);

    virtual bool initBoard();

  public:
    Q1370B();
    float getTemperature(int index);
    virtual void requestChipTemps();
};
