#pragma once

#include <stdbool.h>

#include "stratum_api.h"


void create_jobs_task(void *pvParameters);
void create_job_mining_notify(mining_notify *notify);

void create_job_set_enonce(char *enonce, int enonce2_len);
bool create_job_set_difficulty(uint32_t diffituly);
void create_job_set_version_mask(uint32_t mask);

