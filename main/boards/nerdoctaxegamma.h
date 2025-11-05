#pragma once

#include "asic.h"
#include "bm1370.h"
#include "board.h"
#include "nerdqaxeplus2.h"
#include "nerdoctaxegamma.h"

// Voltage regulator detection pin (available from hardware rev 3.1+)
// - HIGH (pull-up to 3.3V): TPS53667 regulator
// - LOW (connected to GND): TPS53647 regulator
// - If not connected (older revisions): defaults to TPS53647 (via internal pull-down)
#define VR_DETECT_PIN GPIO_NUM_3

class NerdOctaxeGamma : public NerdQaxePlus2 {
  public:
    NerdOctaxeGamma();
};
