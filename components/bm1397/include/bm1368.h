#ifndef BM1368_H_
#define BM1368_H_

#include "common.h"
#include "driver/gpio.h"
#include "mining.h"
#include "rom/gpio.h"
#include "asic.h"

#define CRC5_MASK 0x1F

#define BM1368_SERIALTX_DEBUG false
#define BM1368_SERIALRX_DEBUG false
#define BM1368_DEBUG_WORK false
#define BM1368_DEBUG_JOBS false

static const uint64_t BM1368_CORE_COUNT = 80;
static const uint64_t BM1368_SMALL_CORE_COUNT = 1276;

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

#endif /* BM1368_H_ */
