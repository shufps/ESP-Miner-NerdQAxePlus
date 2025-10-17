#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <stdbool.h>
#include <string>

#include "esp_log.h"
#include "lvgl.h"
#include "mbedtls/md.h"

#include "../otp/qrcodegen.h"
#include "global_state.h"
#include "macros.h"
#include "otp.h"

/*** RFC4648 Base32 (uppercase, no padding) ***/
static const char B32_ALPH[33] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

static const char *TAG = "OTP";

static bool is_time_synced(void)
{
    time_t now = 0;
    time(&now);
    return (now >= 1609459200); // crude check that RTC is sane
}

bool OTP::parse_otp6(const std::string &s, uint32_t &out)
{
    // Accept exactly 6 digits (ignore spaces)
    out = 0;
    int cnt = 0;
    for (char c : s) {
        if (c == ' ')
            continue;
        if (!std::isdigit((unsigned char) c))
            return false;
        out = out * 10u + (uint32_t) (c - '0');
        ++cnt;
        if (cnt > 6)
            return false;
    }
    return cnt == 6;
}

/*** Base32 (RFC4648, uppercase, no padding) decode; returns bytes written or 0 on error ***/
size_t OTP::base32_decode(const char *in, size_t inlen, uint8_t *out, size_t outcap)
{
    auto val = [](int ch) -> int {
        if (ch >= 'A' && ch <= 'Z')
            return ch - 'A';
        if (ch >= 'a' && ch <= 'z')
            return ch - 'a';
        if (ch >= '2' && ch <= '7')
            return ch - '2' + 26;
        return -1;
    };
    uint32_t buf = 0;
    int bits = 0;
    size_t o = 0;
    for (size_t i = 0; i < inlen; ++i) {
        int v = val((unsigned char) in[i]);
        if (v < 0)
            continue; // skip whitespace/padding if any
        buf = (buf << 5) | (uint32_t) v;
        bits += 5;
        if (bits >= 8) {
            if (o >= outcap)
                return 0;
            out[o++] = (uint8_t) ((buf >> (bits - 8)) & 0xFF);
            bits -= 8;
        }
    }
    return o;
}

/*** HOTP (RFC4226) using HMAC-SHA1 ***/
bool OTP::hotp_sha1(const uint8_t *key, size_t keylen, uint64_t counter, uint32_t &out_code, int digits = 6)
{
    uint8_t msg[8];
    for (int i = 7; i >= 0; --i) {
        msg[i] = (uint8_t) (counter & 0xFF);
        counter >>= 8;
    }

    const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
    if (!info)
        return false;

    uint8_t mac[20];
    if (mbedtls_md_hmac(info, key, keylen, msg, sizeof(msg), mac) != 0)
        return false;

    int off = mac[19] & 0x0F;
    uint32_t bin = ((mac[off] & 0x7F) << 24) | ((mac[off + 1] & 0xFF) << 16) | ((mac[off + 2] & 0xFF) << 8) | (mac[off + 3] & 0xFF);
    uint32_t mod = 1;
    for (int i = 0; i < digits; i++)
        mod *= 10;
    out_code = bin % mod;
    return true;
}

/*** TOTP verify with +/-1 window and 3-bit replay mask ***/
bool OTP::totp_verify_window(const uint8_t *key, size_t keylen, time_t now, int period, int digits, uint32_t user_code,
                             int64_t &io_base_step, uint8_t &io_mask)
{
    int64_t step = now / period;

    // Slide window forward if time advanced beyond base+1
    if (io_base_step == 0 || step > io_base_step + 1) {
        io_base_step = step;
        io_mask = 0;
    } else if (step < io_base_step - 1) {
        return false; // far too old
    }

    for (int off = -1; off <= 1; ++off) {
        int64_t s = step + off;
        int idx = (int) (s - (io_base_step - 1)); // 0,1,2 map to base-1, base, base+1
        if (idx < 0 || idx > 2)
            continue;
        if (io_mask & (1u << idx))
            continue; // replay

        uint32_t code = 0;
        if (!hotp_sha1(key, keylen, (uint64_t) s, code, digits))
            return false;
        if (code == user_code) {
            io_mask |= (1u << idx); // mark used
            return true;
        }
    }
    return false;
}

OTP::OTP() : m_mutex(PTHREAD_MUTEX_INITIALIZER), m_enrollmentActive(false), m_isEnabled(false)
{}

bool OTP::init()
{
    Board *board = SYSTEM_MODULE.getBoard();

    if (!board) {
        ESP_LOGE(TAG, "board is null");
        return false;
    }

    m_hostname = SYSTEM_MODULE.getHostname();
    m_mac = SYSTEM_MODULE.getMacAddress();
    m_deviceModel = board->getDeviceModel();

    // load saved values
    loadSettings();

    return true;
}

void OTP::loadSettings()
{
    m_isEnabled = Config::isOTPEnabled();
    m_secretBase32 = Config::getOTPSecret();
}

void OTP::saveSettings()
{
    Config::setOTPSecret(m_secretBase32.c_str());
    Config::setOTPEnabled(m_isEnabled);
    Config::setOTPReplayState(0, 0);
}

std::string OTP::base32_encode(const uint8_t *in, size_t len)
{
    std::string out;
    out.reserve((len * 8 + 4) / 5);
    uint32_t buf = 0;
    int bits = 0;
    for (size_t i = 0; i < len; ++i) {
        buf = (buf << 8) | in[i];
        bits += 8;
        while (bits >= 5) {
            out.push_back(B32_ALPH[(buf >> (bits - 5)) & 0x1F]);
            bits -= 5;
        }
    }
    if (bits > 0) {
        out.push_back(B32_ALPH[(buf << (5 - bits)) & 0x1F]);
    }
    return out;
}

/*** Minimal URL-encode for label/issuer (space -> %20 etc.) ***/
inline bool OTP::is_unreserved(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' ||
           c == '~';
}

std::string OTP::url_encode(const std::string &s)
{
    static const char HEX[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(s.size() * 3);
    for (unsigned char c : s) {
        if (is_unreserved((char) c))
            out.push_back((char) c);
        else {
            out.push_back('%');
            out.push_back(HEX[c >> 4]);
            out.push_back(HEX[c & 0xF]);
        }
    }
    return out;
}

/*** Build a compact otpauth:// URI (Authy/GA defaults implied: SHA1, 6 digits, 30s) ***/
std::string OTP::build_otpauth_uri(const std::string &label_raw, const std::string &issuer_raw, const std::string &secret_b32)
{
    std::string label = url_encode(label_raw);
    std::string issuer = url_encode(issuer_raw);

    // Keep it short: omit algorithm/digits/period (they are defaults)
    std::string uri = "otpauth://totp/" + issuer + ":" + label + "?secret=" + secret_b32 + "&issuer=" + issuer;
    ESP_LOGE(TAG, "%s", uri.c_str());
    return uri;
}

std::string OTP::createURI()
{
    std::string label = m_hostname + "-" + m_mac[0] + m_mac[1] + m_mac[3] + m_mac[4];

    // Build compact otpauth:// URI (keep label/issuer short)
    return build_otpauth_uri(label, m_deviceModel, m_secretBase32);
}

std::string OTP::createSecret()
{
    std::array<uint8_t, 20> secret{};

    // create secret with hardware RNG
    esp_fill_random(secret.data(), secret.size());

    // Base32 encode for otpauth URI
    return base32_encode(secret.data(), secret.size());
}



bool OTP::validate(const std::string &token)
{
    PThreadGuard lock(m_mutex);

    if (!isEnabled()) {
        ESP_LOGI(TAG, "otp not enabled");
        return true;
    }

    // 1) Parse 6-digit code
    uint32_t user_code = 0;
    if (!parse_otp6(token, user_code)) {
        ESP_LOGE(TAG, "TOTP parse failed");
        return false;
    }

    // 2) Ensure we have a Base32 secret (load from NVS if needed)
    if (m_secretBase32.empty()) {
        ESP_LOGE(TAG, "No TOTP secret");
        return false;
    }

    // 3) Decode Base32 -> binary key
    uint8_t key[32];
    size_t klen = base32_decode(m_secretBase32.c_str(), m_secretBase32.size(), key, sizeof(key));
    if (klen == 0) {
        ESP_LOGE(TAG, "Base32 decode failed");
        return false;
    }

    // 4) Load replay-state from NVS (base_step + mask), defaults 0
    int64_t base_step = 0;
    uint8_t used_mask = 0;
    Config::getOTPReplayState(base_step, used_mask);

    // 5) Current time
    if (!is_time_synced()) {
        ESP_LOGE(TAG, "System time not set");
        return false;
    }

    time_t now = 0;
    time(&now);

    // 6) Verify with +/-1 window and replay protection
    bool ok = totp_verify_window(key, klen, now, /*period=*/30, /*digits=*/6, user_code, base_step, used_mask);
    if (!ok) {
        ESP_LOGE(TAG, "invalid otp");
        return false;
    }
    ESP_LOGI(TAG, "otp verified!");

    // 7) Persist updated replay-state (atomic enough for this use-case)
    Config::setOTPReplayState(base_step, (uint8_t) (used_mask & 0x07));

    return true;
}

static inline size_t qr_buf_len_for_maxver(int maxVer)
{
    return qrcodegen_BUFFER_LEN_FOR_VERSION(maxVer);
}

void OTP::destroyQrCode()
{
    safe_free(m_qrTmpBuf);
    safe_free(m_qrBuf);
}

// Create QR into PSRAM using C API; keeps result in m_qrBuf/m_qrSize
bool OTP::createNewQrCode()
{
    if (m_isEnabled) {
        ESP_LOGE(TAG, "OTP enabled, can't create new QR");
        return false;
    }

    // 1) Make a fresh secret and URI
    m_secretBase32 = createSecret();
    m_uri = createURI();

    // 2) Allocate buffers in PSRAM (max version 10 keeps QR small & readable on 170px)
    const int minVer = qrcodegen_VERSION_MIN;
    const int maxVer = 10; // pick 10 as safe upper bound; increase if URIs get longer
    const bool boostEcl = true;

    const size_t need = qr_buf_len_for_maxver(maxVer);
    if (!m_qrTmpBuf)
        m_qrTmpBuf = (uint8_t *) MALLOC(need);
    if (!m_qrBuf)
        m_qrBuf = (uint8_t *) MALLOC(need);
    if (!m_qrTmpBuf || !m_qrBuf) {
        ESP_LOGE(TAG, "QR PSRAM alloc failed (need=%u)", (unsigned) need);
        return false;
    }

    // 3) Encode (UTF-8 text)
    const bool ok = qrcodegen_encodeText(m_uri.c_str(), m_qrTmpBuf, m_qrBuf, qrcodegen_Ecc_MEDIUM, minVer, maxVer,
                                         qrcodegen_Mask_AUTO, boostEcl);

    if (!ok) {
        ESP_LOGE(TAG, "qrcodegen_encodeText failed (URI too long for maxVer=%d?)", maxVer);
        return false;
    }

    // 4) Store size (modules per side)
    m_qrSize = qrcodegen_getSize(m_qrBuf);
    ESP_LOGI(TAG, "QR ready: size=%d modules", m_qrSize);

    // Reset replay-state for fresh secret
    Config::setOTPReplayState(0, 0);
    return true;
}

esp_err_t OTP::startEnrollment()
{
    PThreadGuard lock(m_mutex);

    if (m_enrollmentActive) {
        ESP_LOGE(TAG, "enrollment already active");
        return ESP_FAIL;
    }
    m_enrollmentActive = true;

    if (!createNewQrCode()) {
        ESP_LOGE(TAG, "qr code couldn't be generated");
        return ESP_FAIL;
    }

    return ESP_OK;
}
