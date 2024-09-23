#ifndef SYSTEM_H_
#define SYSTEM_H_

#include "global_state.h"

void SYSTEM_task(void *parameters);

void SYSTEM_notify_accepted_share();
void SYSTEM_notify_rejected_share();
void SYSTEM_notify_found_nonce(double pool_diff, int asic_nr);
void SYSTEM_check_for_best_diff(double found_diff, uint8_t job_id);
void SYSTEM_notify_mining_started();
void SYSTEM_notify_new_ntime(uint32_t ntime);

#endif /* SYSTEM_H_ */
