#pragma once

#include "asic.h"
#include "bm1370.h"
#include "board.h"
#include "nerdqaxeplus.h"

class NerdQaxePlus2 : public NerdQaxePlus {
  protected:
    void LDO_enable();
    void LDO_disable();

  public:
    NerdQaxePlus2();
    float getTemperature(int index);
};