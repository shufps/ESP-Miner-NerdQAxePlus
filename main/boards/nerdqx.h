#pragma once

#include "asic.h"
#include "bm1370.h"
#include "board.h"
#include "nerdqaxeplus2.h"
#include "nerdoctaxegamma.h"
#include "./drivers/tmp451_mux.h"

class NerdQX : public NerdQaxePlus2 {
  protected:
    Tmp451Mux m_tmp451;

    // flag to remember if we found the tmux
    bool m_hasTMux = false;

  public:
    NerdQX();
    virtual bool initBoard();
    virtual void requestChipTemps();
};