#pragma once

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

#include "asic.h"
#include "bm1368.h"
#include "tasks/asic_jobs.h"
#include "tasks/power_management_task.h"
#include "stratum/stratum_manager.h"
#include "stratum/stratum_manager_dual_pool.h"
#include "stratum/stratum_manager_fallback.h"
#include "tasks/apis_task.h"

#include "boards/nerdqaxeplus.h"
#include "system.h"
#include "discord.h"
#include "hashrate_monitor_task.h"
#include "otp/otp.h"
#include "http_server/handler_ota_factory.h"

extern System SYSTEM_MODULE;
extern PowerManagementTask POWER_MANAGEMENT_MODULE;
extern HashrateMonitor HASHRATE_MONITOR;

extern StratumManager *STRATUM_MANAGER;
extern APIsFetcher APIs_FETCHER;
extern FactoryOTAUpdate FACTORY_OTA_UPDATER;

extern AsicJobs asicJobs;
extern DiscordAlerter discordAlerter;

extern OTP otp;

uint64_t now_ms();
uint32_t now();
bool is_time_synced(void);
