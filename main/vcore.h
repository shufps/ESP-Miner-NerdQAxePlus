#ifndef VCORE_H_
#define VCORE_H_

#include "global_state.h"

void VCORE_init(float volts, GlobalState *global_state);
void VCORE_LDO_enable(GlobalState *global_state);
bool VCORE_set_voltage(float core_voltage, GlobalState *global_state);
uint16_t VCORE_get_voltage_mv(GlobalState *global_state);
void VCORE_LDO_disable(GlobalState *global_state);
#endif /* VCORE_H_ */
