#pragma once

class Alerter {
  protected:
    static constexpr uint32_t messageBufferSize = 700;
    static constexpr uint32_t payloadBufferSize = 768;

    char* m_messageBuffer = nullptr;
    char* m_payloadBuffer = nullptr;

  public:
    Alerter();

    virtual void init();

    virtual void loadConfig() = 0;
    virtual bool sendTestMessage() = 0;
    virtual bool sendMessage(const char* message) = 0;
};

class DiscordAlerter : public Alerter {
  protected:
    char *m_webhookUrl = nullptr;

    bool sendRaw(const char* message);

  public:
    DiscordAlerter();

    virtual void init();
    virtual void loadConfig();
    virtual bool sendTestMessage();
    virtual bool sendMessage(const char* message);
    virtual bool sendWatchdogAlert();
    virtual bool sendBlockFoundAlert(double diff, double networkDiff);
};
