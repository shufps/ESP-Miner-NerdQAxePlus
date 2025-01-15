#pragma once

enum eTimeProvider {
  NTP,
  NTIME
};

class TimeProvider {
  protected:
    // selected provider
    eTimeProvider m_timeProvider;

    // NTP host
    const char* m_NTPHost;

    // last time the clock was synced via ntime
    uint32_t m_lastNTimeClockSync;

    // flag if we have synced the clock
    bool m_synced;

  public:
    TimeProvider();

    static void taskWrapper(void *pvParameters);
    void task();

    void setNTime(uint32_t ntime);

    bool isSynced()
    {
        return m_synced;
    }
};