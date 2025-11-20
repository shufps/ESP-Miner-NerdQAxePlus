#pragma once

#include "stratum_manager.h"

class StratumManagerFallback : public StratumManager {
    friend StratumTask; ///< Allows StratumTask to access private members

  protected:
    int m_selected = 0;
    uint64_t m_accepted = 0;
    uint64_t m_rejected = 0;
    uint64_t m_bestSessionDiff = 0;

    virtual void reconnectTimerCallback(int index);
    virtual void connectedCallback(int index);
    virtual void disconnectedCallback(int index);

    virtual bool acceptsNotifyFrom(int pool);

    virtual void acceptedShare(int pool)
    {
        m_accepted++;
    }

    virtual void rejectedShare(int pool)
    {
        m_rejected++;
    }

  public:
    StratumManagerFallback();

    virtual const char *getCurrentPoolHost();
    virtual int getCurrentPoolPort();

    virtual const char *getResolvedIpForSelected() const;

    virtual int getNextActivePool();

    void loadSettings();

    virtual uint32_t selectAsicDiff(int pool, uint32_t poolDiff, uint32_t asicMin, uint32_t asicMax);

    virtual void checkForBestDiff(int pool, double diff, uint32_t nbits);

    virtual void getManagerInfoJson(JsonObject &obj);

    virtual bool isUsingFallback()
    {
        return m_selected == StratumManager::Selected::SECONDARY;
    }
};
