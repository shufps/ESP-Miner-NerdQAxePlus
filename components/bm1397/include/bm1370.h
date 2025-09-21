#pragma once

#include "driver/gpio.h"
#include "mining.h"
#include "rom/gpio.h"
#include "bm1368.h"

class BM1370 : public Asic {
protected:
    virtual const uint8_t* getChipId();
    virtual uint32_t getDefaultVrFrequency();

    virtual uint8_t jobToAsicId(uint8_t job_id);
    virtual uint8_t asicToJobId(uint8_t asic_id);

    virtual uint8_t nonceToAsicNr(uint32_t nonce);
    virtual uint8_t chipIndexFromAddr(uint8_t addr);
    virtual uint8_t addrFromChipIndex(uint8_t idx);
public:
    BM1370();
    virtual const char* getName() { return "BM1370"; };
    virtual uint8_t init(uint64_t frequency, uint16_t asic_count, uint32_t difficulty, uint32_t vrFrequency);
    virtual void resetCounter(uint8_t reg);
    virtual void readCounter(uint8_t reg);
    virtual uint16_t getSmallCoreCount();
};

