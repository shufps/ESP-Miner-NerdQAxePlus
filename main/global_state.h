#pragma once

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

#include "stratum_api.h"

#include "asic.h"
#include "bm1368.h"
#include "tasks/asic_jobs.h"
#include "tasks/power_management_task.h"
#include "tasks/stratum_task.h"

#include "boards/nerdqaxeplus.h"
#include "system.h"

extern System SYSTEM_MODULE;
extern PowerManagementTask POWER_MANAGEMENT_MODULE;
extern StratumTask STRATUM_TASK;

extern AsicJobs asicJobs;

