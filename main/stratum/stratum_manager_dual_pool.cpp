#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>

#include "create_jobs_task.h"
#include "macros.h"
#include "nvs_config.h"
#include "stratum_manager_dual_pool.h"

StratumManagerDualPool::StratumManagerDualPool(int balance) : StratumManager(PoolMode::DUAL), m_primary_pct(balance)
{
    // NOP
}

const char *StratumManagerDualPool::getResolvedIpForSelected() const
{
    // what to do? is used for ping TODO
    return nullptr;
}

void StratumManagerDualPool::reconnectTimerCallback(int index)
{
    // dual pool: both pools always try to stay alive
    m_stratumTasks[index]->connect();
}

void StratumManagerDualPool::connectedCallback(int index)
{
    return;
}

void StratumManagerDualPool::disconnectedCallback(int index)
{
    PThreadGuard lock(m_mutex);
    create_job_invalidate(index);
    m_stratumTasks[index]->m_validNotify = false;
    m_stratumTasks[index]->startReconnectTimer();
}

int StratumManagerDualPool::getNextActivePool()
{
    PThreadGuard lock(m_mutex);
    int secondary_pct = 100 - m_primary_pct;

    bool valid0 = m_stratumTasks[0] && m_stratumTasks[0]->m_validNotify;
    bool valid1 = m_stratumTasks[1] && m_stratumTasks[1]->m_validNotify;

    // fast paths: only one pool has valid work
    if (valid0 && !valid1) {
        m_error_accum = 0; // reset drift so wir nicht "Schuld" ansammeln
        return PRIMARY;
    }
    if (!valid0 && valid1) {
        m_error_accum = 0;
        return SECONDARY;
    }
    if (!valid0 && !valid1) {
        // nobody valid -> doesn't matter, but don't explode error
        m_error_accum = 0;
        return PRIMARY;
    }

    // dithering logic
    m_error_accum += secondary_pct; // accumulate pressure for SECONDARY

    if (m_error_accum >= 100) {
        // time to give SECONDARY a turn
        m_error_accum -= 100;
        return SECONDARY;
    }
    return PRIMARY;
}

const char *StratumManagerDualPool::getCurrentPoolHost()
{
    // TODO
    return "-";
}

int StratumManagerDualPool::getCurrentPoolPort()
{
    // TODO
    return 0;
}

uint32_t StratumManagerDualPool::selectAsicDiff(int pool, uint32_t poolDiff, uint32_t asicMin, uint32_t asicMax)
{
    static uint32_t poolDiffs[2] = {0xffffffffu, 0xffffffffu};

    // shouldn't happen
    if (pool < 0 || pool >= 2) {
        return asicMax;
    }
/*
    if (poolDiff < asicMin) {
        if (!pool) {
            m_errorFlags |= ErrorFlags::POOLDIFF_0;
        } else {
            m_errorFlags |= ErrorFlags::POOLDIFF_1;
        }
    }
*/
    poolDiffs[pool] = poolDiff;

    uint32_t minDiff = std::min(poolDiffs[0], poolDiffs[1]);

    // clamp to ASIC range
    if (minDiff < asicMin) {
        return asicMin;
    }
    if (minDiff > asicMax) {
        return asicMax;
    }
    return minDiff;
}

bool StratumManagerDualPool::acceptsNotifyFrom(int pool)
{
    return true;
}

void StratumManagerDualPool::loadSettings()
{
    PThreadGuard lock(m_mutex);

    // call parent
    StratumManager::loadSettings();

    // set new percentage and reset error
    int new_pct = Config::getPoolBalance();
    if (new_pct != m_primary_pct) {
        m_primary_pct = new_pct;
        m_error_accum = 0; // reset dithering to avoid drift from old config
    }
};
