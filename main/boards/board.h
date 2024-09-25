#pragma once

#include "asic.h"
#include "bm1368.h"
#include "nvs_config.h"

class Board {
  protected:

    const char *device_model;
    int version;
    const char *asic_model;
    int asic_count;
    float asic_job_frequency_ms;
    float asic_frequency;
    float asic_voltage;
    uint32_t asic_initial_difficulty;
    bool fan_invert_polarity;
    float fan_perc;

    virtual Asic* get_asics() = 0;
  public:
    Board();

    virtual bool init() = 0;

    void load_settings();
    const char *get_device_model();
    int get_version();
    const char *get_asic_model();
    int get_asic_count();
    double get_asic_job_frequency_ms();
    uint32_t get_initial_ASIC_difficulty();

    // abstract common methos
    virtual bool set_voltage(float core_voltage) = 0;
    virtual uint16_t get_voltage_mv() = 0;

    virtual void set_fan_speed(float perc) = 0;
    virtual void get_fan_speed(uint16_t *rpm) = 0;

    virtual float read_temperature(int index) = 0;

    virtual float get_vin() = 0;
    virtual float get_iin() = 0;
    virtual float get_pin() = 0;
    virtual float get_vout() = 0;
    virtual float get_iout() = 0;
    virtual float get_pout() = 0;

    bool asic_proccess_work(task_result *result);
    int asic_set_max_baud(void);
    void asic_set_job_difficulty_mask(uint32_t mask);
    uint8_t asic_send_work(uint32_t job_id, bm_job *next_bm_job);
    bool asic_send_hash_frequency(float frequency);
};

