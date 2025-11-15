#pragma once

#include "stratum_manager.h"

class StratumManagerDualPool : public StratumManager {
    friend StratumTask; ///< Allows StratumTask to access private members

  protected:
    int m_primary_pct = 50;
    int32_t m_error_accum = 0;

    virtual void reconnectTimerCallback(int index);
    virtual void connectedCallback(int index);
    virtual void disconnectedCallback(int index);

    virtual bool acceptsNotifyFrom(int pool);

  public:
    StratumManagerDualPool(int balance);

    virtual const char *getCurrentPoolHost();
    virtual int getCurrentPoolPort();

    virtual const char *getResolvedIpForSelected() const;

    virtual uint32_t selectAsicDiff(uint32_t poolDiff, uint32_t asicMin, uint32_t asicMax);

    virtual int getNextActivePool();

    virtual void loadSettings();

    virtual bool isUsingFallback()
    {
        // not applicable
        return false;
    }
};
