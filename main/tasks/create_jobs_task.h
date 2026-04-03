#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "stratum/stratum_api.h"
#include "mining.h"

/**
 * @brief Abstract base class for protocol-agnostic mining job construction.
 *
 * Derived classes implement protocol-specific job building (V1 coinbase+merkle,
 * V2 standard channel, V2 extended channel) while the create_jobs_task loop
 * remains protocol-agnostic.
 */
class MiningInfoBase {
  public:
    virtual ~MiningInfoBase();

    virtual bm_job* buildBmJob(uint32_t extranonce_2, int pool_id, uint32_t asic_diff) = 0;
    virtual bool isValid() const = 0;
    virtual bool isNewWork(uint32_t &last_ntime) const = 0;
    virtual const char* getJobId() const = 0;
    virtual uint32_t getActiveDifficulty() const = 0;
    virtual uint32_t getVersionMask() const = 0;
    virtual void invalidate() = 0;
};

// Global mining info instances - one per pool slot, protocol-polymorphic
extern MiningInfoBase* miningInfo[2];

// Mutex for protecting miningInfo access
extern pthread_mutex_t current_stratum_job_mutex;

// Main task entry point
void create_jobs_task(void *pvParameters);

// Trigger immediate job creation
void trigger_job_creation();

// V1-specific free functions (called from StratumManager::dispatch)
void create_job_mining_notify(int pool, mining_notify *notify, bool abandonWork);
void create_job_set_enonce(int pool, char *enonce, int enonce2_len);
void set_next_enonce(int pool, char *enonce, int enonce2_len);
bool create_job_set_difficulty(int pool, uint32_t difficulty);
void create_job_set_version_mask(int pool, uint32_t mask);
void create_job_invalidate(int pool);
