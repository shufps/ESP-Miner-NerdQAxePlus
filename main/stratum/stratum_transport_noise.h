#pragma once

#include "stratum_transport.h"

extern "C" {
#include "sv2_noise.h"
}

/**
 * @brief Noise-encrypted transport for Stratum V2.
 *
 * Wraps a plain TCP connection with Noise_NX handshake and
 * ChaCha20-Poly1305 encryption/decryption for all subsequent I/O.
 */
class NoiseStratumTransport : public StratumTransport {
  public:
    NoiseStratumTransport();
    ~NoiseStratumTransport() override;

    bool connect(const char *host, const char *ip, uint16_t port) override;
    int send(const void *data, size_t len) override;
    int recv(void *buf, size_t len) override;
    void close() override;

    /// Set optional authority pubkey (32-byte x-only) for server cert verification.
    void setAuthorityPubkey(const uint8_t pubkey[32]);
    void clearAuthorityPubkey();

    /// Get the underlying esp_transport handle (needed for Noise I/O).
    esp_transport_handle_t getTransportHandle() { return m_t; }

    /// Get the Noise context (needed for SV2 frame send/recv in StratumTaskV2).
    sv2_noise_ctx_t *getNoiseCtx() { return m_noise_ctx; }

  private:
    sv2_noise_ctx_t *m_noise_ctx = nullptr;
    uint8_t m_authority_pubkey[32];
    bool m_has_authority_pubkey = false;
};
