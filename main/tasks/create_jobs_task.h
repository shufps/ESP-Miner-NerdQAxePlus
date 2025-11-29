#pragma once

#include <stdbool.h>

#include "stratum/stratum_api.h"


void create_jobs_task(void *pvParameters);

void create_job_mining_notify(int pool, mining_notify *notify, bool abandonWork);
void create_job_set_enonce(int pool, char *enonce, int enonce2_len);
void set_next_enonce(int pool, char *enonce, int enonce2_len);
bool create_job_set_difficulty(int pool, uint32_t difficulty);
void create_job_set_version_mask(int pool, uint32_t mask);
void create_job_invalidate(int pool);
