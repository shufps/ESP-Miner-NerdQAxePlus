#pragma once

#include "stratum_manager.h"

class StratumManagerDualPool : public StratumManager {
    friend StratumTask; ///< Allows StratumTask to access private members

  protected:
    int m_primary_pct = 50;
    int32_t m_error_accum = 0;

    uint64_t m_accepted[2]{};
    uint64_t m_rejected[2]{};
    uint64_t m_bestSessionDiff[2]{};
    bool m_poolDiffErr[2]{};

    virtual void reconnectTimerCallback(int index);
    virtual void connectedCallback(int index);
    virtual void disconnectedCallback(int index);

    virtual bool acceptsNotifyFrom(int pool);

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

    virtual bool isUsingFallback()
    {
        // not applicable
        return false;
    }
};
