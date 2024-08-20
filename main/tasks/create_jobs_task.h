#ifndef CREATE_JOBS_TASK_H_
#define CREATE_JOBS_TASK_H_

#include "stratum_api.h"
#include <stdbool.h>

void create_jobs_task(void *pvParameters);
void create_job_mining_notify(mining_notify *notify);

void create_job_set_enonce(char *enonce, int enonce2_len);
bool create_job_set_difficulty(uint32_t diffituly);
void create_job_set_version_mask(uint32_t mask);

#endif