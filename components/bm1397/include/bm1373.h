#pragma once

#include "bm1370.h"

class BM1373 : public BM1370 {
protected:
    virtual const uint8_t* getChipId();
    virtual uint32_t getDefaultVrFrequency();

public:
    BM1373();
    virtual const char* getName() { return "BM1373"; };
    virtual uint8_t init(uint64_t frequency, uint16_t asic_count, uint32_t difficulty, uint32_t vrFrequency);
    virtual uint16_t getSmallCoreCount();
};
