#pragma once

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

#define ESP_LOGIE(b, tag, fmt, ...)                                                                                                \
    do {                                                                                                                           \
        if (b) {                                                                                                                   \
            ESP_LOGI(tag, fmt, ##__VA_ARGS__);                                                                                     \
        } else {                                                                                                                   \
            ESP_LOGE(tag, fmt, ##__VA_ARGS__);                                                                                     \
        }                                                                                                                          \
    } while (0)

typedef struct
{
    uint8_t job_id;
    uint32_t nonce;
    uint32_t rolled_version;
    int asic_nr;
    uint32_t data;
    uint8_t reg;
    uint8_t is_reg_resp;
} task_result;

typedef struct __attribute__((__packed__))
{
    uint8_t preamble[2];
    uint32_t nonce;
    uint8_t midstate_num;
    uint8_t job_id;
    uint16_t version;
    uint8_t crc;
} asic_result_t;

class Asic {
protected:
    float m_current_frequency;
    float m_actual_current_frequency;

    void send(uint8_t header, uint8_t *data, uint8_t data_len, bool debug);
    void send2(uint8_t header, uint8_t b0, uint8_t b1);
    void send6(uint8_t header, uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5);
    int count_asics();
    bool sendHashFrequency(float target_freq);
    bool doFrequencyTransition(float target_frequency);
    void setChipAddress(uint8_t chipAddr);
    void sendReadAddress(void);
    void sendChainInactive(void);
    uint16_t reverseUint16(uint16_t num);
    bool receiveWork(asic_result_t *result);

    // asic model specific
    virtual const uint8_t* getChipId() = 0;
    virtual uint8_t jobToAsicId(uint8_t job_id) = 0;
    virtual uint8_t asicToJobId(uint8_t asic_id) = 0;

public:
    Asic();
    virtual const char* getName() = 0;
    uint8_t sendWork(uint32_t job_id, bm_job *next_bm_job);
    bool processWork(task_result *result);
    void setJobDifficultyMask(int difficulty);
    bool setAsicFrequency(float frequency);
    virtual void requestChipTemp() = 0;
    virtual uint16_t getSmallCoreCount() = 0;
    virtual uint8_t nonceToAsicNr(uint32_t nonce) = 0;

    // asic models specific
    virtual uint8_t init(uint64_t frequency, uint16_t asic_count, uint32_t difficulty) = 0;
    virtual int setMaxBaud(void) = 0;
};
