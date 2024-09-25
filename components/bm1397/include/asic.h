#pragma once

#include "common.h"
#include "mining.h"

#define CRC5_MASK 0x1F

#define ASIC_SERIALTX_DEBUG false
#define ASIC_SERIALRX_DEBUG false
#define ASIC_DEBUG_WORK false
#define ASIC_DEBUG_JOBS false

#define TYPE_JOB 0x20
#define TYPE_CMD 0x40

#define GROUP_SINGLE 0x00
#define GROUP_ALL 0x10

#define CMD_JOB 0x01

#define CMD_SETADDRESS 0x00
#define CMD_WRITE 0x01
#define CMD_READ 0x02
#define CMD_INACTIVE 0x03

#define CMD_WRITE_SINGLE (TYPE_CMD | GROUP_SINGLE | CMD_WRITE)
#define CMD_WRITE_ALL    (TYPE_CMD | GROUP_ALL | CMD_WRITE)
#define CMD_READ_ALL    (TYPE_CMD | GROUP_ALL | CMD_READ)


#define RESPONSE_CMD 0x00
#define RESPONSE_JOB 0x80

#define SLEEP_TIME 20
#define FREQ_MULT 25.0

#define CLOCK_ORDER_CONTROL_0 0x80
#define CLOCK_ORDER_CONTROL_1 0x84
#define ORDERED_CLOCK_ENABLE 0x20
#define CORE_REGISTER_CONTROL 0x3C
#define PLL3_PARAMETER 0x68
#define FAST_UART_CONFIGURATION 0x28
#define TICKET_MASK 0x14
#define MISC_CONTROL 0x18

class Asic {
protected:
    float current_frequency;

    void send(uint8_t header, uint8_t *data, uint8_t data_len, bool debug);
    void send2(uint8_t header, uint8_t b0, uint8_t b1);
    void send6(uint8_t header, uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5);
    int count_asics();
    bool send_hash_frequency(float target_freq);
    bool do_frequency_transition(float target_frequency);
    void set_chip_address(uint8_t chipAddr);
    void send_read_address(void);
    void send_chain_inactive(void);
    uint16_t reverse_uint16(uint16_t num);
    bool receive_work(asic_result_t *result);

    // asic model specific
    virtual const uint8_t* get_chip_id() = 0;
    virtual uint8_t job_to_asic_id(uint8_t job_id) = 0;
    virtual uint8_t asic_to_job_id(uint8_t asic_id) = 0;

public:
    Asic();
    uint8_t send_work(uint32_t job_id, bm_job *next_bm_job);
    bool proccess_work(task_result *result);
    void set_job_difficulty_mask(int difficulty);
    bool set_hash_frequency(float frequency);

    // asic models specific
    virtual uint8_t init(uint64_t frequency, uint16_t asic_count, uint32_t difficulty) = 0;
    virtual int set_max_baud(void) = 0;
};