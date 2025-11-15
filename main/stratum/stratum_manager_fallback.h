#pragma once

#include "stratum_manager.h"

class StratumManagerFallback : public StratumManager {
    friend StratumTask; ///< Allows StratumTask to access private members

  protected:
    int m_selected; // default PRIMARY

    virtual void reconnectTimerCallback(int index);
    virtual void connectedCallback(int index);
    virtual void disconnectedCallback(int index);

    virtual bool acceptsNotifyFrom(int pool);

  public:
    StratumManagerFallback();

    virtual const char *getCurrentPoolHost();
    virtual int getCurrentPoolPort();

    virtual const char *getResolvedIpForSelected() const;

    virtual int getNextActivePool();

    virtual uint32_t selectAsicDiff(uint32_t poolDiff, uint32_t asicMin, uint32_t asicMax);

    virtual bool isUsingFallback()
    {
        return m_selected == StratumManager::Selected::SECONDARY;
    }
};
