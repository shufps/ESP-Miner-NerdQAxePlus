#pragma once

#include "asic.h"
#include "bm1370.h"
#include "board.h"
#include "nerdqaxeplus2.h"
#include "./drivers/tmp451_mux.h"

// Voltage regulator detection pin (available from hardware rev 3.1+)
// - HIGH (pull-up to 3.3V): TPS53667 regulator
// - LOW (connected to GND): TPS53647 regulator
// - If not connected (older revisions): defaults to TPS53647 (via internal pull-down)
#define VR_DETECT_PIN GPIO_NUM_3

class NerdOctaxeGamma : public NerdQaxePlus2 {
  protected:
    // TMP451 mux chips – only present on rev 3.4
    // [0]: ASICs 0–3 (I2C addr 0x4c)
    // [1]: ASICs 4–7 (I2C addr 0x4e)
    // Both chips share the same MUX select lines: GPIO2=A0, GPIO12=A1.
    Tmp451Mux m_tmp451[2];
    bool      m_hasTMux[2] = {false, false};

  public:
    NerdOctaxeGamma();
    virtual bool initBoard() override;
    virtual void requestChipTemps() override;
    float getVRTemp() override;

  private:
    bool m_isTPS53667 = false;
};
