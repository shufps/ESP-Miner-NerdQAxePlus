#include "stratum_transport_noise.h"

#include "esp_log.h"

#include "macros.h"

extern "C" {
#include "sv2_noise.h"
#include "sv2_protocol.h"
}

static const char *TAG = "noise_transport";

NoiseStratumTransport::NoiseStratumTransport()
    : StratumTransport(false) // TCP, not TLS
{
}

NoiseStratumTransport::~NoiseStratumTransport()
{
    close();
}

void NoiseStratumTransport::setAuthorityPubkey(const uint8_t pubkey[32])
{
    memcpy(m_authority_pubkey, pubkey, 32);
    m_has_authority_pubkey = true;
}

void NoiseStratumTransport::clearAuthorityPubkey()
{
    m_has_authority_pubkey = false;
    memset(m_authority_pubkey, 0, 32);
}

bool NoiseStratumTransport::connect(const char *host, const char *ip, uint16_t port)
{
    // First establish plain TCP connection via base class
    if (!StratumTransport::connect(host, ip, port)) {
        return false;
    }

    // Create Noise context; publish it to m_noise_ctx only after the
    // handshake succeeded so a concurrent send() can't use it mid-handshake
    sv2_noise_ctx_t *ctx = sv2_noise_create();
    if (!ctx) {
        ESP_LOGE(TAG, "Failed to create Noise context");
        StratumTransport::close();
        return false;
    }

    // Perform Noise_NX handshake
    ESP_LOGI(TAG, "Starting Noise handshake");
    int ret = sv2_noise_handshake(ctx, m_t,
                                  m_has_authority_pubkey ? m_authority_pubkey : nullptr);
    if (ret != 0) {
        ESP_LOGE(TAG, "Noise handshake failed");
        sv2_noise_destroy(ctx);
        StratumTransport::close();
        return false;
    }

    {
        PThreadGuard g(m_lock);
        m_noise_ctx = ctx;
    }

    ESP_LOGI(TAG, "Encrypted channel established (ChaCha20-Poly1305)");
    return true;
}

int NoiseStratumTransport::send(const void *data, size_t len)
{
    PThreadGuard g(m_lock);

    if (!m_noise_ctx || !m_t) {
        return -1;
    }

    int ret = sv2_noise_send(m_noise_ctx, m_t, (const uint8_t *)data, (int)len);
    return (ret == 0) ? (int)len : -1;
}

int NoiseStratumTransport::recv(void *buf, size_t len)
{
    // Note: For SV2, we don't use this generic recv() method directly.
    // Instead, StratumTaskV2 uses sv2_noise_recv() which returns
    // header + payload separately. This method exists for interface
    // compatibility but SV2 uses the Noise context directly.
    (void)buf;
    (void)len;
    ESP_LOGW(TAG, "Generic recv() not used for SV2 — use sv2_noise_recv() directly");
    return -1;
}

void NoiseStratumTransport::close()
{
    // unblock a submit that may be stuck inside sv2_noise_send() so it
    // releases m_lock before we destroy the noise ctx under it
    shutdownSocket_();

    PThreadGuard g(m_lock);
    if (m_noise_ctx) {
        sv2_noise_destroy(m_noise_ctx);
        m_noise_ctx = nullptr;
    }
    StratumTransport::close();
}
