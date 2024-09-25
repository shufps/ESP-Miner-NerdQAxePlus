#pragma once

#include "driver/gpio.h"
#include "mining.h"
#include "rom/gpio.h"
#include "asic.h"

class BM1368 : public Asic {
protected:
    virtual const uint8_t* get_chip_id();

    virtual uint8_t job_to_asic_id(uint8_t job_id);
    virtual uint8_t asic_to_job_id(uint8_t asic_id);

public:
    BM1368();
    virtual uint8_t init(uint64_t frequency, uint16_t asic_count, uint32_t difficulty);
    virtual int set_max_baud(void);
};

