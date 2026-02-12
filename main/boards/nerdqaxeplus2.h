#pragma once

#include "asic.h"
#include "bm1370.h"
#include "board.h"
#include "nerdqaxeplus.h"

class NerdQaxePlus2 : public NerdQaxePlus {
  public:
    NerdQaxePlus2();
    float getTemperature(int index);
    virtual void requestChipTemps();
};
