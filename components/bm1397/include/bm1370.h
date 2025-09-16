#pragma once

#include "driver/gpio.h"
#include "mining.h"
#include "rom/gpio.h"
#include "bm1368.h"

class BM1370 : public BM1368 {
protected:
    virtual const uint8_t* getChipId();
    virtual uint32_t getDefaultVrFrequency();
public:
    BM1370();
    virtual const char* getName() { return "BM1370"; };
    virtual uint8_t init(uint64_t frequency, uint16_t asic_count, uint32_t difficulty, uint32_t vrFrequency);
    virtual void requestChipTemp();
    virtual uint16_t getSmallCoreCount();
    virtual uint8_t nonceToAsicNr(uint32_t nonce);
};

