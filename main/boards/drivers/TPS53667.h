#pragma once

#include "TPS53647.h"

class TPS53667 : public TPS53647 {
protected:
    virtual void set_phases(int num_phases);

public:
    static constexpr uint16_t DEVICE_CODE = 0x01f8;

    TPS53667();

    virtual bool init(int num_phases, int imax, float ifault);

    virtual void status();
};
