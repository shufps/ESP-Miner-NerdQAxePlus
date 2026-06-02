#pragma once

#include <stdint.h>

class BuckConverter {
  public:
    virtual ~BuckConverter() = default;

    virtual bool init(int num_phases, int imax, float ifault) = 0;
    virtual void clear_faults() = 0;

    virtual float get_temperature() = 0;
    virtual float get_pin() = 0;
    virtual float get_pout() = 0;
    virtual float get_vin() = 0;
    virtual float get_iin() = 0;
    virtual float get_iout() = 0;
    virtual float get_vout() = 0;
    virtual bool uses_external_vr_temperature() { return true; }

    virtual bool set_vout(float volts) = 0;
    virtual bool disable_vout() { return true; }

    virtual void status() = 0;

    virtual uint8_t get_status_byte() = 0;
    virtual uint8_t get_status_iout() = 0;
    virtual uint8_t get_status_vout() = 0;
    virtual uint8_t get_status_input() = 0;
    virtual uint8_t get_status_temp() = 0;
};
