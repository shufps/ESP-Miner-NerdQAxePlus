#pragma once

class SNTP {
  protected:
    bool waitForInitialSync(int timeout_ms);
  public:
    SNTP();

    void start();
    void logLocalTime();
};