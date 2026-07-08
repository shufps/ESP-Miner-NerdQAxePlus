#pragma once

#include "q1370.h"

class Q1373B : public Q1370B {
  protected:
    // board options detected via strap resistors on the FXL6408;
    // defaults match boards without straps (4 phases, ethernet)
    bool m_hasEth = true;
    bool m_is6Phase = false;

    // VR chip detected via PMBus device code
    bool m_isTPS53667 = false;

    void detectBoardOptions();

  public:
    Q1373B();
    bool initBoard() override;
    bool hasEthernet() override { return m_hasEth; }
};
