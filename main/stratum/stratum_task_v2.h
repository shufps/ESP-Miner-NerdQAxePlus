#pragma once

#include "stratum_task.h"
#include "stratum_transport_noise.h"

extern "C" {
#include "sv2_protocol.h"
}

#define SV2_MAX_FRAME_SIZE 2048

class StratumManager;

/**
 * @brief Stratum V2 task - handles binary SV2 protocol with Noise encryption.
 *
 * Connection lifecycle:
 *   TCP connect → Noise handshake → SetupConnection → OpenChannel →
 *   receive loop (NewMiningJob/NewExtendedMiningJob + SetNewPrevHash) →
 *   submit shares
 */
class StratumTaskV2 : public StratumTaskBase {
    friend StratumManager;

  protected:
    NoiseStratumTransport m_noiseTransport;
    sv2_conn_t m_sv2_conn;           ///< SV2 connection state (channel, pending jobs, etc.)
    sv2_channel_type_t m_channelType; ///< Standard vs Extended
    int64_t m_lastSubmitTimeUs = 0;   ///< For response time measurement

    // StratumTaskBase overrides
    void protocolLoop() override;
    void submitShare(const char *jobid, const char *extranonce_2, const uint32_t ntime,
                     const uint32_t nonce, const uint32_t version_rolled, const uint32_t version_base) override;
    StratumTransport *selectTransport() override;

  private:
    // SV2 protocol handshake steps
    bool sendSetupConnection();
    bool receiveSetupConnectionSuccess();
    bool sendOpenChannel();
    bool receiveOpenChannelSuccess();

    // SV2 message handlers
    void handleNewMiningJob(const uint8_t *payload, uint32_t len);
    void handleNewExtendedMiningJob(const uint8_t *payload, uint32_t len);
    void handleSetNewPrevHash(const uint8_t *payload, uint32_t len);
    void handleSetTarget(const uint8_t *payload, uint32_t len);
    void handleSubmitSharesSuccess(const uint8_t *payload, uint32_t len);
    void handleSubmitSharesError(const uint8_t *payload, uint32_t len);

    // Job delivery to create_jobs_task pipeline
    void enqueueStandardJob(uint32_t job_id, uint32_t version,
                            const uint8_t merkle_root[32], const uint8_t prev_hash[32],
                            uint32_t ntime, uint32_t nbits, bool clean);
    void enqueueExtendedJob(sv2_ext_job_t *job);

    // Helper to load authority pubkey from NVS
    bool loadAuthorityPubkey(uint8_t out[32]);

    // Frame buffers
    uint8_t m_frameBuf[SV2_MAX_FRAME_SIZE];
    uint8_t m_recvBuf[SV2_MAX_FRAME_SIZE];
    uint8_t m_hdrBuf[SV2_FRAME_HEADER_SIZE];

  public:
    StratumTaskV2(StratumManager *manager, int index);
};
