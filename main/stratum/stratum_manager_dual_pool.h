#pragma once

#include "stratum_manager.h"

class StratumManagerDualPool : public StratumManager {
    friend StratumTask; ///< Allows StratumTask to access private members

  protected:
    int m_balance = 50;
    int32_t m_error_accum = 0;

    uint64_t m_accepted[2]{};
    uint64_t m_rejected[2]{};
    uint64_t m_bestSessionDiff[2]{};
    bool m_poolDiffErr[2]{};
    uint32_t m_poolDifficulty[2]{0};

    virtual void reconnectTimerCallback(int index);
    virtual void connectedCallback(int index);
    virtual void disconnectedCallback(int index);

    virtual bool acceptsNotifyFrom(int pool);

    virtual void setPoolDifficulty(int pool, uint32_t diff) {
        m_poolDifficulty[pool] = diff;
    };

    virtual void acceptedShare(int pool)
    {
        m_accepted[pool]++;
    }

    virtual void rejectedShare(int pool)
    {
        m_rejected[pool]++;
    }

  public:
    StratumManagerDualPool();

    bool getPoolDiffErr(int i) {
        if (i < 0 || i >= 2) {
            return false;
        }
        return m_poolDiffErr[i];
    }

    virtual const char *getCurrentPoolHost();
    virtual int getCurrentPoolPort();

    virtual const char *getResolvedIpForSelected() const;

    virtual uint32_t selectAsicDiff(int pool, uint32_t poolDiff, uint32_t asicMin, uint32_t asicMax);

    virtual int getNextActivePool();

    virtual void loadSettings();

    virtual void checkForBestDiff(int pool, double diff, uint32_t nbits);

    virtual void getManagerInfoJson(JsonObject &obj);

    // aggregated compatibility methos
    virtual uint64_t getSharesAccepted() {
        return m_accepted[0] + m_accepted[1];
    }

    virtual uint64_t getSharesRejected() {
        return m_rejected[0] + m_rejected[1];
    }

    virtual uint32_t getPoolDifficulty() {
        return (m_balance >= 50) ? m_poolDifficulty[0] : m_poolDifficulty[1];
    };

    virtual uint64_t getBestSessionDiff() {
        return std::max(m_bestSessionDiff[0], m_bestSessionDiff[1]);
    }

    virtual int getPoolErrors() {
        return m_stratumTasks[0]->m_poolErrors + m_stratumTasks[1]->m_poolErrors;
    }

    virtual bool isUsingFallback()
    {
        // not applicable
        return false;
    }
};
