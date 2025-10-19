#pragma once

#include <pthread.h>
#include <string>

#include "lvgl.h"
#include "qrcodegen.h"

class OTP {
  private:
    pthread_mutex_t m_mutex;
    std::string m_hostname;
    std::string m_mac;
    std::string m_deviceModel;
    std::string m_uri;
    std::string m_secretBase32;
    bool m_enrollmentActive;

    // QR buffers in PSRAM
    uint8_t *m_qrTmpBuf = nullptr;
    uint8_t *m_qrBuf = nullptr;
    int m_qrSize = 0;

    // otp session
    uint32_t m_bootId;
    uint8_t m_sessKey[32];
    bool m_hasSessKey;

    bool m_isEnabled;

    // base32
    std::string base32_encode(const uint8_t *in, size_t len);
    size_t base32_decode(const char *in, size_t inlen, uint8_t *out, size_t outcap);

    bool hmac_sha256(const uint8_t *key, size_t keylen, const uint8_t *msg, size_t mlen, uint8_t mac32[32]);

    bool is_unreserved(char c);

    // otp functions
    bool parse_otp6(const std::string &s, uint32_t &out);
    bool hotp_sha1(const uint8_t *key, size_t keylen, uint64_t counter, uint32_t &out_code, int digits);
    bool totp_verify_window(const uint8_t *key, size_t keylen, time_t now, int period, int digits, uint32_t user_code,
                            int64_t &io_base_step, uint8_t &io_mask);

    std::string url_encode(const std::string &s);
    std::string build_otpauth_uri(const std::string &label_raw, const std::string &issuer_raw, const std::string &secret_b32);
    std::string createURI();
    std::string createSecret();
    bool createNewQrCode();

  public:
    OTP();
    bool init();

    bool isInitialized()
    {
        return m_secretBase32 != "";
    }

    void loadSettings();
    void saveSettings();

    void setEnable(bool state)
    {
        m_isEnabled = state;
    }

    bool isEnabled()
    {
        return m_isEnabled;
    }

    bool isEnrollmentActive()
    {
        return m_enrollmentActive;
    }

    uint8_t *getQrCode(int *size)
    {
        *size = m_qrSize;
        return m_qrBuf;
    }

    esp_err_t startEnrollment();

    void disableEnrollment()
    {
        m_enrollmentActive = false;
    }

    void destroyQrCode();

    bool validate(const std::string &token);
    bool verifySessionToken(const std::string &token);
    std::string mintSessionToken(uint32_t ttl_sec /*= 24h*/);
};