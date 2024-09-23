#pragma once

#include "nerdqaxeplus.h"

typedef struct {
    const char* device_model;
    int version;
    const char* asic_model;
    int asic_count;
    float asic_job_frequency_ms;
    float asic_frequency;
    float asic_voltage;
    uint32_t asic_initial_difficulty;
    bool fan_invert_polarity;
    float fan_perc;
} board_t;

bool board_init();

void board_load_settings();

const char *board_get_device_model();

int board_get_version();

const char *board_get_asic_model();

int board_get_asic_count();

double board_get_asic_job_frequency_ms();

uint32_t board_get_initial_ASIC_difficulty();

void board_LDO_enable();
void board_LDO_disable();
bool board_set_voltage(float core_voltage);
uint16_t board_get_voltage_mv();

void board_set_fan_speed(float perc);
void board_get_fan_speed(uint16_t* rpm);

float board_read_temperature(int index);

float board_get_vin();
float board_get_iin();
float board_get_pin();
float board_get_vout();
float board_get_iout();
float board_get_pout();

uint8_t board_asic_init(uint64_t frequency);
bool board_asic_proccess_work(task_result *result);
int board_asic_set_max_baud(void);
void board_asic_set_job_difficulty_mask(uint32_t mask);
uint8_t board_asic_send_work(uint32_t job_id, bm_job *next_bm_job);
bool board_asic_send_hash_frequency(float frequency);
