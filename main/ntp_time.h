#pragma once

class NTPTime {
  protected:
    bool m_valid;

    void initialize_sntp(const char *host);

  public:
    NTPTime();

    static void taskWrapper(void* pvParameters);
    void task();

    bool isValid()
    {
        return m_valid;
    }
};