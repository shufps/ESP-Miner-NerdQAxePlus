#pragma once

#include "esp_http_server.h"

// --- Redirect context (per-client, no globals)
typedef struct {
    bool capture;
    char loc[4096];
} redirect_ctx_t;


class FactoryOTAUpdate {

public:
    // --- Progress/step state (protected by m_mutex)
    enum class OtaStep : uint8_t {
        IDLE = 0,
        RESOLVING_URL,
        UPDATING_FW,
        DOWNLOADING_WWW,
        UPDATING_WWW,
        REBOOTING,
        ERROR
    };

    typedef struct { bool pending; bool running; OtaStep step; int progress; } OtaStatus;

protected:
    pthread_mutex_t m_mutex;
    pthread_cond_t m_cond;
    bool m_running = false;
    bool m_pending = false;

    char *m_update_url = nullptr;

    uint32_t m_fw_written = 0;
    uint32_t m_www_written = 0;
    uint32_t m_www_recv = 0;
    OtaStep  m_step = OtaStep::IDLE;

    esp_err_t http_read_exact(esp_http_client_handle_t &client, uint8_t *dst, int len);
    esp_err_t http_read_chunk(esp_http_client_handle_t &client, uint8_t *dst);
    esp_err_t http_read_chunks(esp_http_client_handle_t &client, uint8_t *dst, int chunks);
    esp_err_t http_skip_chunks(esp_http_client_handle_t &client, int chunks);
    esp_err_t follow_redirect(esp_http_client_handle_t client, char* url, int url_len, redirect_ctx_t *ctx);

    esp_err_t do_www_update(uint8_t *data);
    esp_err_t do_firmware_update(esp_http_client_handle_t client);
    esp_err_t ota_update_from_factory(const char *start_url);

    // Helpers
    void setStep(OtaStep s, const char *logmsg = nullptr);
    void resetProgress();
    void addFwBytes(uint32_t n);
    void addWwwBytes(uint32_t n);
    void addWwwRecvBytes(uint32_t n);
    int  calcProgress() const; // 0..100
public:
    FactoryOTAUpdate();

    static void taskWrapper(void *pvParameters);
    void task();

    bool trigger(const char *url);
    void getStatus(OtaStatus *out);
};

esp_err_t POST_OTA_update_from_url(httpd_req_t *req);
esp_err_t GET_OTA_status(httpd_req_t *req);