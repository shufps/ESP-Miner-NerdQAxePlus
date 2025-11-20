#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>

#include "create_jobs_task.h"
#include "macros.h"
#include "stratum_manager_fallback.h"
#include "utils.h"

StratumManagerFallback::StratumManagerFallback() : StratumManager(PoolMode::FAILOVER), m_selected(PRIMARY)
{
    // NOP
}

void StratumManagerFallback::reconnectTimerCallback(int index)
{
    PThreadGuard lock(m_mutex);

    // failover mode:
    if (index == PRIMARY) {
        // primary is always allowed to reconnect
        m_stratumTasks[index]->connect();
    } else {
        // secondary is only allowed if primary is NOT connected
        if (!isConnected(PRIMARY)) {
            m_stratumTasks[index]->connect();
        } else {
            // primary is good, we don't want secondary online
            m_stratumTasks[index]->disconnect();
        }
    }
}

void StratumManagerFallback::connectedCallback(int index)
{
    PThreadGuard lock(m_mutex);

    if (index == PRIMARY) {
        // Primary is up → Secondary should go away
        m_selected = PRIMARY;

        if (m_stratumTasks[SECONDARY]) {
            m_stratumTasks[SECONDARY]->disconnect();
            m_stratumTasks[SECONDARY]->stopReconnectTimer();
        }
    } else { // index == SECONDARY
        // Secondary is up
        if (!isConnected(PRIMARY)) {
            // Primary is dead → accept Secondary as active
            m_selected = SECONDARY;
        } else {
            // Primary is alive → we don't allow Secondary online
            m_stratumTasks[SECONDARY]->disconnect();
            m_stratumTasks[SECONDARY]->stopReconnectTimer();
        }
    }
}

void StratumManagerFallback::disconnectedCallback(int index)
{
    PThreadGuard lock(m_mutex);
    create_job_invalidate(index);

    m_stratumTasks[index]->m_validNotify = false;

    if (index == PRIMARY) {
        // Primary went down -> allow secondary to try
        if (m_stratumTasks[SECONDARY]) {
            m_stratumTasks[SECONDARY]->startReconnectTimer();
            // m_stratumTasks[SECONDARY]->connect();
        }
        // m_selected will switch to SECONDARY once SECONDARY actually connects
    } else { // index == SECONDARY
        // Secondary went down. If Primary is still down too,
        // we might eventually reconnect SECONDARY anyway via its timer,
    }
}

int StratumManagerFallback::getNextActivePool()
{
    PThreadGuard lock(m_mutex);
    return m_selected;
}

const char *StratumManagerFallback::getCurrentPoolHost()
{
    if (!m_stratumTasks[m_selected]) {
        return "-";
    }
    return m_stratumTasks[m_selected]->getHost();
}

int StratumManagerFallback::getCurrentPoolPort()
{
    if (!m_stratumTasks[m_selected]) {
        return 0;
    }
    return m_stratumTasks[m_selected]->getPort();
}

uint32_t StratumManagerFallback::selectAsicDiff(int pool, uint32_t poolDiff, uint32_t asicMin, uint32_t asicMax)
{
    return std::max(std::min(poolDiff, asicMax), asicMin);
}

bool StratumManagerFallback::acceptsNotifyFrom(int pool)
{
    return (pool == m_selected);
}

void StratumManagerFallback::loadSettings()
{
    PThreadGuard lock(m_mutex);

    StratumManager::loadSettings();
};


void StratumManagerFallback::checkForBestDiff(int pool, double diff, uint32_t nbits)
{
    PThreadGuard lock(m_mutex);

    if ((uint64_t) diff > m_bestSessionDiff) {
        m_bestSessionDiff = (uint64_t) diff;
        suffixString((uint64_t)diff, m_bestSessionDiffString, DIFF_STRING_SIZE, 0);
    }

    StratumManager::checkForBestDiff(pool, diff, nbits);
}

void StratumManagerFallback::getManagerInfoJson(JsonObject &obj) {
    PThreadGuard lock(m_mutex);

    if (!isInitialized()) {
        return;
    }

    StratumManager::getManagerInfoJson(obj);

    // fallback specific
    obj["usingFallback"] = isUsingFallback();

    JsonArray arr = obj["pools"].to<JsonArray>();

    // Objekt IM Array erzeugen, nicht lokal
    JsonObject pool = arr.add<JsonObject>();

    pool["connected"] = m_stratumTasks[m_selected]->m_isConnected;
    pool["poolDifficulty"] = m_poolDifficulty;
    pool["poolDiffErr"] = false;
    pool["accepted"] = m_accepted;
    pool["rejected"] = m_rejected;
    pool["pingRtt"]  = m_pingTasks[m_selected]->get_last_ping_rtt();
    pool["pingLoss"] = m_pingTasks[m_selected]->get_recent_ping_loss();
    pool["bestDiff"] = m_bestSessionDiff;
}

