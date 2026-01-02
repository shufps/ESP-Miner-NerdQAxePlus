#include "esp_log.h"
#include <strings.h>

#include "ArduinoJson.h"
#include "esp_err.h"

#include "http_utils.h"
#include "macros.h"

static const char *TAG = "http_utils";

extern OTP otp;


#define RL_FAIL_LIMIT      5            // allowed wrong tries in window
#define RL_WINDOW_SEC      60           // 1 min window
#define RL_BLOCK_SEC       300          // 5 min blocking time

static uint64_t timestamps[RL_FAIL_LIMIT] = {};
static uint64_t block_exp_time = 0;

// return true if currently blocked, false otherwise
static bool isBlocked() {
    uint64_t ts = now_ms(); // current time in ms

    // if block_exp_time is set and lies in the future -> we are blocked
    if (block_exp_time && block_exp_time > ts) {
        // Do NOT extend the block on further failures.
        // OTP changes after each 30s, so resetting the expiry
        // on subsequent failures is unnecessary.
        return true;
    }
    return false;
}

// most pragmatic approach of /some/ bruteforce protection
static bool rateLimit() {
    uint64_t ts = now_ms(); // current time in ms

    // If a block existed but has expired, clear it and reset failure history.
    if (block_exp_time && block_exp_time <= ts) {
        block_exp_time = 0;
        memset(timestamps, 0, sizeof(timestamps));
    }

    // Rotate the timestamps array one position towards the end (keep newest at index 0).
    // Walk backwards to avoid overwriting values before they are copied.
    for (int i = RL_FAIL_LIMIT - 1; i > 0; i--) {
        timestamps[i] = timestamps[i - 1];
    }
    timestamps[0] = ts; // store newest failure timestamp at index 0

    // If we have a full buffer of failure timestamps, check the time span
    // between the newest and the oldest entry. If it's within the window,
    // set the block expiry and deny further attempts.
    if (timestamps[RL_FAIL_LIMIT - 1] != 0) {
        uint64_t delta = timestamps[0] - timestamps[RL_FAIL_LIMIT - 1]; // newest - oldest
        if (delta < RL_WINDOW_SEC * 1000ull) {
            // block for RL_BLOCK_SEC (store expiry as ms)
            block_exp_time = ts + (RL_BLOCK_SEC * 1000ull);
            // optional: clear history immediately to avoid repeated re-blocking logic
            memset(timestamps, 0, sizeof(timestamps));
            return false; // blocked
        }
    }

    return true; // allowed
}



struct HttpdChunkHeapWriter {
    httpd_req_t* m_req = nullptr;
    bool m_failed = false;

    uint8_t* m_buf = nullptr;
    size_t m_cap = 0;
    size_t m_pos = 0;

    explicit HttpdChunkHeapWriter(httpd_req_t* req, size_t capacity)
        : m_req(req), m_cap(capacity) {
        m_buf = static_cast<uint8_t*>(MALLOC(m_cap));
        if (!m_buf) {
            m_failed = true;
            m_cap = 0;
        }
    }

    ~HttpdChunkHeapWriter() {
        if (m_buf) {
            FREE(m_buf);
            m_buf = nullptr;
        }
    }

    size_t write(uint8_t c) {
        if (m_failed) return 0;
        if (m_pos >= m_cap) {
            flush();
            if (m_failed) return 0;
        }
        m_buf[m_pos++] = c;
        return 1;
    }

    size_t write(const uint8_t* data, size_t len) {
        if (m_failed) return 0;

        size_t written = 0;
        while (len > 0 && !m_failed) {
            const size_t space = m_cap - m_pos;
            if (space == 0) {
                flush();
                continue;
            }

            const size_t n = (len < space) ? len : space;
            memcpy(&m_buf[m_pos], data, n);
            m_pos += n;

            data += n;
            len -= n;
            written += n;

            if (m_pos == m_cap) {
                flush();
            }
        }
        return written;
    }

    void flush() {
        if (m_failed || m_pos == 0) return;

        const esp_err_t err = httpd_resp_send_chunk(
            m_req,
            reinterpret_cast<const char*>(m_buf),
            m_pos
        );
        if (err != ESP_OK) {
            m_failed = true;
            return;
        }
        m_pos = 0;
    }

    esp_err_t finish() {
        flush();
        if (m_failed) return ESP_FAIL;
        return httpd_resp_send_chunk(m_req, nullptr, 0);
    }
};

esp_err_t sendJsonResponse(httpd_req_t* req, JsonDocument& doc)
{
    HttpdChunkHeapWriter w(req, 2048);
    if (w.m_failed) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return ESP_FAIL;
    }

    serializeJson(doc, w);
    return w.finish();
}

esp_err_t getPostData(httpd_req_t *req)
{
    int total_len = req->content_len;
    int cur_len = 0;
    char *buf = ((rest_server_context_t *) (req->user_ctx))->scratch;
    int received = 0;

    if (total_len >= SCRATCH_BUFSIZE) {
        /* Respond with 500 Internal Server Error */
        ESP_LOGE(TAG, "content too long");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }

    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len);
        if (received <= 0) {
            /* Respond with 500 Internal Server Error */
            ESP_LOGE(TAG, "error receiving data");
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "error receiving data");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';
    return ESP_OK;
}

esp_err_t getJsonData(httpd_req_t *req, JsonDocument &doc)
{
    esp_err_t err = getPostData(req);
    if (err != ESP_OK) {
        return err;
    }

    char *buf = ((rest_server_context_t *) (req->user_ctx))->scratch;

    // Parse the JSON payload
    DeserializationError error = deserializeJson(doc, buf);
    if (error) {
        ESP_LOGE(TAG, "JSON parsing failed: %s", error.c_str());
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    return ESP_OK;
}

// --- helpers ---------------------------------------------------------------
static bool read_header_str(httpd_req_t *req, const char *name, char *out, size_t outlen)
{
    size_t len = httpd_req_get_hdr_value_len(req, name);
    if (len == 0 || len + 1 > outlen)
        return false;
    return httpd_req_get_hdr_value_str(req, name, out, outlen) == ESP_OK;
}

static bool read_totp_header(httpd_req_t *req, char *out, size_t outlen)
{
    return read_header_str(req, "X-TOTP", out, outlen);
}

static bool read_session_token(httpd_req_t *req, char *out, size_t outlen)
{
    return read_header_str(req, "X-OTP-Session", out, outlen);
}

static bool check_otp_or_session(httpd_req_t *req, bool force)
{
    if (!otp.isEnabled() && !force)
        return true;

    // example token
    // LGQ7I2GZ6L2WRCJXHJHA.7NGNYMKM5MK6WVI3NJHTOGL2NWPFI6SSHHFQTN6SJPEQ5WQVQZGA
    // 20 (payload) + 1 + 52 (bas32 sha256) = 73
    if (!force) {
        char sess[128] = {0};
        if (read_session_token(req, sess, sizeof(sess))) {
            if (otp.verifySessionToken(std::string(sess))) {
                return true;
            }
            // fallback to totp
        }
    }

    // totp
    char totp[16] = {0};
    if (read_totp_header(req, totp, sizeof(totp))) {
        if (otp.validate(totp))
            return true;
    }

    return false;
}

// Bequeme Variante: sendet 401 + WWW-Authenticate bei Failure
esp_err_t validateOTP(httpd_req_t *req, bool force)
{
    if (isBlocked()) {
        httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "blocked for 5 minutes");
        return ESP_FAIL;
    }

    if (check_otp_or_session(req, force))
        return ESP_OK;

    ESP_LOGE(TAG, "totp validation failed");

    // sinnvolle Fehlerantwort zentral
    //httpd_resp_set_hdr(req, "WWW-Authenticate", "Bearer realm=\"NerdQX\", error=\"invalid_token\"");
    httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "OTP/Session required");

    // record the failed attempt
    if (!rateLimit()) {
        ESP_LOGE(TAG, "too many OTP failures. Blocking ...");
    }

    return ESP_FAIL;
}
