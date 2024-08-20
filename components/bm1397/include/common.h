#ifndef COMMON_H_
#define COMMON_H_

#include <stdint.h>

typedef struct __attribute__((__packed__))
{
    uint8_t job_id;
    uint32_t nonce;
    uint32_t rolled_version;
} task_result;

typedef enum
{
    JOB_PACKET = 0,
    CMD_PACKET = 1,
} packet_type_t;

typedef enum
{
    JOB_RESP = 0,
    CMD_RESP = 1,
} response_type_t;

unsigned char _reverse_bits(unsigned char num);
int _largest_power_of_two(int num);

#endif