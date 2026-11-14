#pragma once

#include "coinbase_decoder.h"

#define ALERTER_MESSAGE_LEN 512

typedef struct {
    char message[ALERTER_MESSAGE_LEN];
} alerter_msg_t;

class Alerter {
  protected:
    static constexpr uint32_t payloadBufferSize = 1024;

    char* m_payloadBuffer = nullptr;

    QueueHandle_t m_msgQueue = nullptr;

    char *m_webhookUrl = nullptr;
    char *m_host = nullptr;
    bool m_wdtAlertEnabled = false;
    bool m_blockFoundAlertEnabled = false;
    bool m_bestDiffAlertEnabled = false;
    bool m_coinbaseVerifyAlertEnabled = false;
    uint16_t m_heartbeatHours = 0;        // 0 = disabled

    virtual bool init();

    virtual bool httpPost(const char* message) = 0;
    virtual bool enqueueMessage(const char *message) = 0;
  public:
    Alerter();
    virtual void start() = 0;
    virtual void loadConfig();
    virtual bool sendTestMessage() = 0;
    virtual bool sendWatchdogAlert() = 0;
    virtual bool sendBlockFoundAlert(double diff, double networkDiff) = 0;
    virtual bool sendBestDifficultyAlert(double diff, double networkDiff) = 0;
    virtual bool sendCoinbaseVerifyFailed(int pool, const coinbase_result_t *cb, int mode) = 0;
    virtual bool sendHeartbeat() = 0;
};

class DiscordAlerter : public Alerter {
  protected:

    virtual bool init();

    static void taskWrapper(void *pv);
    void task();
    void checkHeartbeat();

    uint16_t m_activeHeartbeatHours = 0;  // interval the timer below was started for
    int64_t m_lastHeartbeatUs = 0;

    virtual bool httpPost(const char* message);
    virtual bool enqueueMessage(const char *message);
  public:
    DiscordAlerter();

    virtual void start();
    virtual bool sendTestMessage();
    virtual bool sendWatchdogAlert();
    virtual bool sendBlockFoundAlert(double diff, double networkDiff);
    virtual bool sendBestDifficultyAlert(double diff, double networkDiff);
    virtual bool sendCoinbaseVerifyFailed(int pool, const coinbase_result_t *cb, int mode);
    virtual bool sendHeartbeat();
};
