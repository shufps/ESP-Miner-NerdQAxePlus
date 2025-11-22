#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>

#include "global_state.h"
#include "create_jobs_task.h"
#include "macros.h"
#include "nvs_config.h"
#include "stratum_manager_dual_pool.h"
#include "utils.h"

StratumManagerDualPool::StratumManagerDualPool() : StratumManager(PoolMode::DUAL)
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
    PThreadGuard lock(m_mutex);

    // dual pool: both pools always try to stay alive
    m_stratumTasks[index]->connect();
}

void StratumManagerDualPool::connectedCallback(int index)
{
    PThreadGuard lock(m_mutex);

    // reset poolDiffErr
    if (index >= 0 && index < 2) {
        m_poolDiffErr[index] = false;
    }
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
    int secondary_pct = 100 - m_balance;

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

uint32_t StratumManagerDualPool::selectAsicDiff(int pool, uint32_t poolDiff)
{
    Board *board = SYSTEM_MODULE.getBoard();
    uint32_t asicMax = board->getAsicMaxDifficulty();
    uint32_t asicMin = board->getAsicMinDifficultyDualPool();

    static uint32_t poolDiffs[2] = {0xffffffffu, 0xffffffffu};

    // shouldn't happen
    if (pool < 0 || pool >= 2) {
        return asicMax;
    }

    m_poolDiffErr[pool] = poolDiff < asicMin;

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

    StratumManager::loadSettings();

    // set new percentage and reset error
    int new_pct = Config::getPoolBalance();
    if (new_pct != m_balance) {
        m_balance = new_pct;
        m_error_accum = 0; // reset dithering to avoid drift from old config
    }
};

void StratumManagerDualPool::checkForBestDiff(int pool, double diff, uint32_t nbits)
{
    PThreadGuard lock(m_mutex);

    if (pool < 0 || pool >= 2) {
        return;
    }

    if ((uint64_t) diff > m_bestSessionDiff[pool]) {
        m_bestSessionDiff[pool] = (uint64_t) diff;
        suffixString(std::max(m_bestSessionDiff[0], m_bestSessionDiff[1]), m_bestSessionDiffString, DIFF_STRING_SIZE, 0);
    }

    StratumManager::checkForBestDiff(pool, diff, nbits);
}

void StratumManagerDualPool::getManagerInfoJson(JsonObject &obj)
{
    PThreadGuard lock(m_mutex);

    StratumManager::getManagerInfoJson(obj);

    // dual pool specific
    obj["poolBalance"] = Config::getPoolBalance();

    JsonArray arr = obj["pools"].to<JsonArray>();

    for (int i = 0; i < 2; i++) {
        JsonObject pool = arr.add<JsonObject>();

        pool["connected"] = m_stratumTasks[i] ? m_stratumTasks[i]->m_isConnected : false;
        pool["poolDifficulty"] = m_poolDifficulty[i];
        pool["poolDiffErr"] = m_poolDiffErr[i];
        pool["accepted"] = m_accepted[i];
        pool["rejected"] = m_rejected[i];
        pool["pingRtt"]  = m_pingTasks[i] ? m_pingTasks[i]->get_last_ping_rtt() : 0;
        pool["pingLoss"] = m_pingTasks[i] ? m_pingTasks[i]->get_recent_ping_loss() : 0;
        pool["bestDiff"] = m_bestSessionDiff[i];
    }
}
