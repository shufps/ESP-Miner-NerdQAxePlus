#include "stratum_task_v2.h"
#include "stratum_manager.h"

#include <cstring>
#include <cstdlib>

#include "esp_log.h"
#include "esp_timer.h"

#include "asic_jobs.h"
#include "boards/board.h"
#include "create_jobs_task.h"
#include "create_jobs_sv2.h"
#include "global_state.h"
#include "macros.h"
#include "nvs_config.h"
#include "system.h"

extern "C" {
#include "sv2_protocol.h"
#include "sv2_noise.h"
#include "libbase58.h"
}

static const char *TAG = "stratum_v2";

// ============================================================================
// Construction
// ============================================================================

StratumTaskV2::StratumTaskV2(StratumManager *manager, int index)
    : StratumTaskBase(manager, index)
{
    memset(&m_sv2_conn, 0, sizeof(m_sv2_conn));
    m_channelType = SV2_CHANNEL_EXTENDED; // default
}

StratumTransport *StratumTaskV2::selectTransport()
{
    // Load authority pubkey from NVS and configure transport
    uint8_t auth_key[32];
    if (loadAuthorityPubkey(auth_key)) {
        m_noiseTransport.setAuthorityPubkey(auth_key);
        ESP_LOGI(m_tag, "Authority pubkey configured, will verify server certificate");
    } else {
        m_noiseTransport.clearAuthorityPubkey();
        ESP_LOGW(m_tag, "No authority pubkey configured, server identity will not be verified");
    }

    return &m_noiseTransport;
}

bool StratumTaskV2::loadAuthorityPubkey(uint8_t out[32])
{
    char *b58_key = m_config->isPrimary()
        ? Config::getSV2AuthorityPubkey()
        : Config::getFallbackSV2AuthorityPubkey();

    if (!b58_key || strlen(b58_key) == 0) {
        safe_free(b58_key);
        return false;
    }

    // SV2 authority pubkey format: base58check(0x0001_LE + 32_byte_xonly_pubkey)
    // Decoded: 2-byte version + 32-byte pubkey + 4-byte checksum = 38 bytes
    uint8_t decoded[64];
    size_t decoded_len = sizeof(decoded);

    if (!b58tobin(decoded, &decoded_len, b58_key, 0)) {
        ESP_LOGE(TAG, "Failed to decode base58 authority pubkey");
        safe_free(b58_key);
        return false;
    }
    safe_free(b58_key);

    if (decoded_len != 38) {
        ESP_LOGE(TAG, "Invalid decoded length: %zu (expected 38)", decoded_len);
        return false;
    }

    // b58tobin right-aligns data in the buffer
    uint8_t *data = decoded + (sizeof(decoded) - decoded_len);

    // Verify version (0x0001 in little-endian)
    if (data[0] != 0x01 || data[1] != 0x00) {
        ESP_LOGE(TAG, "Invalid key version: 0x%02x%02x (expected 0x0100)", data[1], data[0]);
        return false;
    }

    memcpy(out, data + 2, 32);
    ESP_LOGI(TAG, "Successfully decoded base58 authority pubkey");
    return true;
}

// ============================================================================
// Protocol Loop (SV2 lifecycle)
// ============================================================================

void StratumTaskV2::protocolLoop()
{
    // Extended Channel only for now - Standard Channel support disabled
    // To re-enable, uncomment the config read below:
    // uint16_t chan_cfg = m_config->isPrimary()
    //     ? Config::getSV2ChannelType()
    //     : Config::getFallbackSV2ChannelType();
    // m_channelType = (chan_cfg == 1) ? SV2_CHANNEL_STANDARD : SV2_CHANNEL_EXTENDED;
    m_channelType = SV2_CHANNEL_EXTENDED;

    // Reset connection state
    memset(&m_sv2_conn, 0, sizeof(m_sv2_conn));
    m_sv2_conn.channel_type = m_channelType;
    m_lastSubmitTimeUs = 0;

    // Set default version mask for version rolling
    // (SV2 uses the same version mask concept as V1)
    // create_job_set_version_mask(m_index, 0x1fffe000);

    ESP_LOGI(m_tag, "SV2 protocol loop starting (channel=%s)",
             m_channelType == SV2_CHANNEL_EXTENDED ? "extended" : "standard");

    // 1. SetupConnection
    if (!sendSetupConnection() || !receiveSetupConnectionSuccess()) {
        ESP_LOGE(m_tag, "SV2 SetupConnection failed");
        return;
    }

    // 2. OpenChannel
    if (!sendOpenChannel() || !receiveOpenChannelSuccess()) {
        ESP_LOGE(m_tag, "SV2 OpenChannel failed");
        return;
    }

    // Connection successful - mark as connected
    connectedCallback();
    m_isConnected = true;

    ESP_LOGI(m_tag, "SV2+Noise connection ready, waiting for jobs");

    // 3. Main receive loop
    while (1) {
        if (m_stopFlag || m_reconnect || POWER_MANAGEMENT_MODULE.isShutdown()) {
            ESP_LOGI(m_tag, "SV2 loop exit requested");
            return;
        }

        int payload_len = 0;
        sv2_noise_ctx_t *noise = m_noiseTransport.getNoiseCtx();
        esp_transport_handle_t transport = m_noiseTransport.getTransportHandle();

        if (!noise || !transport) {
            ESP_LOGE(m_tag, "Noise context or transport lost");
            return;
        }

        if (sv2_noise_recv(noise, transport, m_hdrBuf, m_recvBuf,
                           sizeof(m_recvBuf), &payload_len) != 0) {
            ESP_LOGE(m_tag, "Failed to receive SV2 frame, reconnecting...");
            return;
        }

        sv2_frame_header_t hdr;
        sv2_parse_frame_header(m_hdrBuf, &hdr);

        switch (hdr.msg_type) {
        case SV2_MSG_NEW_MINING_JOB:
            handleNewMiningJob(m_recvBuf, hdr.msg_length);
            break;

        case SV2_MSG_NEW_EXTENDED_MINING_JOB:
            handleNewExtendedMiningJob(m_recvBuf, hdr.msg_length);
            break;

        case SV2_MSG_SET_NEW_PREV_HASH:
            handleSetNewPrevHash(m_recvBuf, hdr.msg_length);
            break;

        case SV2_MSG_SET_TARGET:
            handleSetTarget(m_recvBuf, hdr.msg_length);
            break;

        case SV2_MSG_SUBMIT_SHARES_SUCCESS:
            handleSubmitSharesSuccess(m_recvBuf, hdr.msg_length);
            break;

        case SV2_MSG_SUBMIT_SHARES_ERROR:
            handleSubmitSharesError(m_recvBuf, hdr.msg_length);
            break;

        default:
            ESP_LOGW(m_tag, "Unknown SV2 message type: 0x%02x (len=%lu)",
                     hdr.msg_type, (unsigned long)hdr.msg_length);
            break;
        }
    }
}

// ============================================================================
// SV2 Protocol Handshake
// ============================================================================

bool StratumTaskV2::sendSetupConnection()
{
    Board *board = SYSTEM_MODULE.getBoard();
    const char *device_model = board ? board->getDeviceModel() : "";
    const char *asic_model = board ? board->getAsicModel() : "";
    uint32_t setup_flags = (m_channelType == SV2_CHANNEL_STANDARD) ? 0x01 : 0x00;

    ESP_LOGI(m_tag, "Sending SetupConnection (vendor=%s, hw=%s, channel=%s)",
             device_model ? device_model : "",
             asic_model ? asic_model : "",
             m_channelType == SV2_CHANNEL_EXTENDED ? "extended" : "standard");

    int frame_len = sv2_build_setup_connection(m_frameBuf, sizeof(m_frameBuf),
                                               m_config->getHost(), m_config->getPort(),
                                               device_model ? device_model : "",
                                               asic_model ? asic_model : "",
                                               "", "", setup_flags);
    if (frame_len < 0) {
        ESP_LOGE(m_tag, "Failed to build SetupConnection frame");
        return false;
    }

    sv2_noise_ctx_t *noise = m_noiseTransport.getNoiseCtx();
    esp_transport_handle_t transport = m_noiseTransport.getTransportHandle();

    if (sv2_noise_send(noise, transport, m_frameBuf, frame_len) != 0) {
        ESP_LOGE(m_tag, "Failed to send SetupConnection");
        return false;
    }

    return true;
}

bool StratumTaskV2::receiveSetupConnectionSuccess()
{
    int payload_len = 0;
    sv2_noise_ctx_t *noise = m_noiseTransport.getNoiseCtx();
    esp_transport_handle_t transport = m_noiseTransport.getTransportHandle();

    if (sv2_noise_recv(noise, transport, m_hdrBuf, m_recvBuf,
                       sizeof(m_recvBuf), &payload_len) != 0) {
        ESP_LOGE(m_tag, "Failed to receive SetupConnectionSuccess");
        return false;
    }

    sv2_frame_header_t hdr;
    sv2_parse_frame_header(m_hdrBuf, &hdr);

    if (hdr.msg_type != SV2_MSG_SETUP_CONNECTION_SUCCESS) {
        ESP_LOGE(m_tag, "SetupConnection rejected by pool (msg_type=0x%02x)", hdr.msg_type);
        return false;
    }

    uint16_t used_version;
    uint32_t flags;
    if (sv2_parse_setup_connection_success(m_recvBuf, payload_len, &used_version, &flags) != 0) {
        ESP_LOGE(m_tag, "Failed to parse SetupConnectionSuccess");
        return false;
    }

    ESP_LOGI(m_tag, "Pool accepted connection: SV2 version=%d, flags=0x%08lx",
             used_version, (unsigned long)flags);
    return true;
}

bool StratumTaskV2::sendOpenChannel()
{
    const char *user = m_config->getUser();
    float hash_rate = 1e12;
    int frame_len;

    if (m_channelType == SV2_CHANNEL_EXTENDED) {
        ESP_LOGI(m_tag, "Opening extended mining channel (user=%s)", user ? user : "(empty)");
        frame_len = sv2_build_open_extended_mining_channel(
            m_frameBuf, sizeof(m_frameBuf), 1, user ? user : "", hash_rate, 6);
    } else {
        ESP_LOGI(m_tag, "Opening standard mining channel (user=%s)", user ? user : "(empty)");
        frame_len = sv2_build_open_standard_mining_channel(
            m_frameBuf, sizeof(m_frameBuf), 1, user ? user : "", hash_rate);
    }

    if (frame_len < 0) {
        ESP_LOGE(m_tag, "Failed to build OpenChannel frame");
        return false;
    }

    sv2_noise_ctx_t *noise = m_noiseTransport.getNoiseCtx();
    esp_transport_handle_t transport = m_noiseTransport.getTransportHandle();

    if (sv2_noise_send(noise, transport, m_frameBuf, frame_len) != 0) {
        ESP_LOGE(m_tag, "Failed to send OpenChannel");
        return false;
    }

    return true;
}

bool StratumTaskV2::receiveOpenChannelSuccess()
{
    int payload_len = 0;
    sv2_noise_ctx_t *noise = m_noiseTransport.getNoiseCtx();
    esp_transport_handle_t transport = m_noiseTransport.getTransportHandle();

    if (sv2_noise_recv(noise, transport, m_hdrBuf, m_recvBuf,
                       sizeof(m_recvBuf), &payload_len) != 0) {
        ESP_LOGE(m_tag, "Failed to receive OpenChannelSuccess");
        return false;
    }

    sv2_frame_header_t hdr;
    sv2_parse_frame_header(m_hdrBuf, &hdr);

    uint8_t expected_msg = (m_channelType == SV2_CHANNEL_EXTENDED)
                               ? SV2_MSG_OPEN_EXTENDED_MINING_CHANNEL_SUCCESS
                               : SV2_MSG_OPEN_STANDARD_MINING_CHANNEL_SUCCESS;

    if (hdr.msg_type != expected_msg) {
        ESP_LOGE(m_tag, "OpenChannel rejected by pool (msg_type=0x%02x, expected=0x%02x)",
                 hdr.msg_type, expected_msg);
        return false;
    }

    uint32_t request_id, channel_id, group_channel_id;
    uint8_t target[32];

    if (m_channelType == SV2_CHANNEL_EXTENDED) {
        uint16_t extranonce_size;
        uint8_t extranonce_prefix[32];
        uint8_t extranonce_prefix_len;

        if (sv2_parse_open_extended_channel_success(
                m_recvBuf, payload_len, &request_id, &channel_id, target,
                &extranonce_size, extranonce_prefix, &extranonce_prefix_len,
                &group_channel_id) != 0) {
            ESP_LOGE(m_tag, "Failed to parse OpenExtendedChannelSuccess");
            return false;
        }

        m_sv2_conn.extranonce_size = (uint8_t)extranonce_size;
        m_sv2_conn.extranonce_prefix_len = extranonce_prefix_len;
        memcpy(m_sv2_conn.extranonce_prefix, extranonce_prefix, extranonce_prefix_len);

        ESP_LOGI(m_tag, "Extended channel: extranonce_size=%d, prefix_len=%d",
                 extranonce_size, extranonce_prefix_len);
    } else {
        uint8_t extranonce_prefix[32];
        uint8_t extranonce_prefix_len;

        if (sv2_parse_open_channel_success(
                m_recvBuf, payload_len, &request_id, &channel_id, target,
                extranonce_prefix, &extranonce_prefix_len,
                &group_channel_id) != 0) {
            ESP_LOGE(m_tag, "Failed to parse OpenChannelSuccess");
            return false;
        }
    }

    m_sv2_conn.channel_id = channel_id;
    m_sv2_conn.channel_opened = true;
    memcpy(m_sv2_conn.target, target, 32);

    uint32_t pdiff = sv2_target_to_pdiff(target);
    m_manager->setPoolDifficulty(m_index, pdiff);

    ESP_LOGI(m_tag, "Mining channel opened: channel_id=%lu, type=%s, difficulty=%lu",
             (unsigned long)channel_id,
             m_channelType == SV2_CHANNEL_EXTENDED ? "extended" : "standard",
             (unsigned long)pdiff);

    return true;
}

// ============================================================================
// SV2 Message Handlers
// ============================================================================

void StratumTaskV2::handleNewMiningJob(const uint8_t *payload, uint32_t len)
{
    uint32_t channel_id, job_id, version, min_ntime;
    bool has_min_ntime;
    uint8_t merkle_root[32];

    if (sv2_parse_new_mining_job(payload, len, &channel_id, &job_id,
                                 &has_min_ntime, &min_ntime,
                                 &version, merkle_root) != 0) {
        ESP_LOGE(m_tag, "Failed to parse NewMiningJob");
        return;
    }

    ESP_LOGI(m_tag, "New mining job: id=%lu, version=%08lx, future=%s",
             (unsigned long)job_id, (unsigned long)version, has_min_ntime ? "no" : "yes");

    int slot = job_id % SV2_PENDING_JOBS_SIZE;

    if (has_min_ntime) {
        if (m_sv2_conn.has_prev_hash) {
            enqueueStandardJob(job_id, version, merkle_root,
                               m_sv2_conn.prev_hash, min_ntime,
                               m_sv2_conn.prev_hash_nbits, true);
        } else {
            m_sv2_conn.pending_jobs[slot].job_id = job_id;
            m_sv2_conn.pending_jobs[slot].version = version;
            memcpy(m_sv2_conn.pending_jobs[slot].merkle_root, merkle_root, 32);
            m_sv2_conn.pending_jobs[slot].valid = true;
        }
    } else {
        m_sv2_conn.pending_jobs[slot].job_id = job_id;
        m_sv2_conn.pending_jobs[slot].version = version;
        memcpy(m_sv2_conn.pending_jobs[slot].merkle_root, merkle_root, 32);
        m_sv2_conn.pending_jobs[slot].valid = true;
    }
}

void StratumTaskV2::handleNewExtendedMiningJob(const uint8_t *payload, uint32_t len)
{
    uint32_t channel_id;
    sv2_ext_job_t *job = sv2_parse_new_extended_mining_job(payload, len, &channel_id);
    if (!job) {
        ESP_LOGE(m_tag, "Failed to parse NewExtendedMiningJob");
        return;
    }

    ESP_LOGI(m_tag, "New extended mining job: id=%lu, version=%08lx, merkle_branches=%d",
             (unsigned long)job->job_id, (unsigned long)job->version, job->merkle_path_count);

    int slot = job->job_id % SV2_PENDING_JOBS_SIZE;

    if (job->ntime > 0) {
        // Has min_ntime — this is a current job
        if (m_sv2_conn.has_prev_hash) {
            memcpy(job->prev_hash, m_sv2_conn.prev_hash, 32);
            job->nbits = m_sv2_conn.prev_hash_nbits;
            job->clean_jobs = true;
            enqueueExtendedJob(job);
        } else {
            if (m_sv2_conn.ext_pending_jobs[slot]) {
                sv2_ext_job_free(m_sv2_conn.ext_pending_jobs[slot]);
            }
            m_sv2_conn.ext_pending_jobs[slot] = job;
        }
    } else {
        // Future job — store in pending ring
        if (m_sv2_conn.ext_pending_jobs[slot]) {
            sv2_ext_job_free(m_sv2_conn.ext_pending_jobs[slot]);
        }
        m_sv2_conn.ext_pending_jobs[slot] = job;
    }
}

void StratumTaskV2::handleSetNewPrevHash(const uint8_t *payload, uint32_t len)
{
    uint32_t channel_id, job_id, min_ntime, nbits;
    uint8_t prev_hash[32];

    if (sv2_parse_set_new_prev_hash(payload, len, &channel_id, &job_id,
                                     prev_hash, &min_ntime, &nbits) != 0) {
        ESP_LOGE(m_tag, "Failed to parse SetNewPrevHash");
        return;
    }

    ESP_LOGI(m_tag, "New prev_hash: job_id=%lu, ntime=%lu, nbits=%08lx",
             (unsigned long)job_id, (unsigned long)min_ntime, (unsigned long)nbits);

    // Notify manager of network difficulty
    m_manager->setNetworkDifficulty(m_index, nbits);

    bool first_prev_hash = !m_sv2_conn.has_prev_hash;

    memcpy(m_sv2_conn.prev_hash, prev_hash, 32);
    m_sv2_conn.prev_hash_ntime = min_ntime;
    m_sv2_conn.prev_hash_nbits = nbits;
    m_sv2_conn.has_prev_hash = true;

    int slot = job_id % SV2_PENDING_JOBS_SIZE;

    // Resolve standard channel pending jobs
    if (m_sv2_conn.pending_jobs[slot].valid &&
        m_sv2_conn.pending_jobs[slot].job_id == job_id) {
        enqueueStandardJob(job_id, m_sv2_conn.pending_jobs[slot].version,
                           m_sv2_conn.pending_jobs[slot].merkle_root,
                           prev_hash, min_ntime, nbits, true);
        m_sv2_conn.pending_jobs[slot].valid = false;
    }

    if (first_prev_hash) {
        for (int i = 0; i < SV2_PENDING_JOBS_SIZE; i++) {
            if (m_sv2_conn.pending_jobs[i].valid &&
                m_sv2_conn.pending_jobs[i].job_id != job_id) {
                enqueueStandardJob(m_sv2_conn.pending_jobs[i].job_id,
                                   m_sv2_conn.pending_jobs[i].version,
                                   m_sv2_conn.pending_jobs[i].merkle_root,
                                   prev_hash, min_ntime, nbits, true);
                m_sv2_conn.pending_jobs[i].valid = false;
            }
        }
    }

    // Resolve extended channel pending jobs
    if (m_sv2_conn.ext_pending_jobs[slot] &&
        m_sv2_conn.ext_pending_jobs[slot]->job_id == job_id) {
        sv2_ext_job_t *ext_job = m_sv2_conn.ext_pending_jobs[slot];
        m_sv2_conn.ext_pending_jobs[slot] = nullptr;
        memcpy(ext_job->prev_hash, prev_hash, 32);
        ext_job->ntime = min_ntime;
        ext_job->nbits = nbits;
        ext_job->clean_jobs = true;
        enqueueExtendedJob(ext_job);
    }

    if (first_prev_hash) {
        for (int i = 0; i < SV2_PENDING_JOBS_SIZE; i++) {
            if (m_sv2_conn.ext_pending_jobs[i] &&
                m_sv2_conn.ext_pending_jobs[i]->job_id != job_id) {
                sv2_ext_job_t *ext_job = m_sv2_conn.ext_pending_jobs[i];
                m_sv2_conn.ext_pending_jobs[i] = nullptr;
                memcpy(ext_job->prev_hash, prev_hash, 32);
                ext_job->ntime = min_ntime;
                ext_job->nbits = nbits;
                ext_job->clean_jobs = true;
                enqueueExtendedJob(ext_job);
            }
        }
    }

    // Mark that we have a valid notify (used by manager for pool selection)
    m_validNotify = true;
}

void StratumTaskV2::handleSetTarget(const uint8_t *payload, uint32_t len)
{
    uint32_t channel_id;
    uint8_t max_target[32];

    if (sv2_parse_set_target(payload, len, &channel_id, max_target) != 0) {
        ESP_LOGE(m_tag, "Failed to parse SetTarget");
        return;
    }

    memcpy(m_sv2_conn.target, max_target, 32);
    uint32_t pdiff = sv2_target_to_pdiff(max_target);
    ESP_LOGI(m_tag, "Set pool difficulty: %lu", (unsigned long)pdiff);

    m_manager->setPoolDifficulty(m_index, pdiff);

    // Update difficulty in MiningInfo and force resend for Standard Channel
    // (Bitaxe uses a global pool_difficulty that create_jobs_task reads on each dequeue)
    create_job_sv2_set_difficulty(m_index, pdiff);
}

void StratumTaskV2::handleSubmitSharesSuccess(const uint8_t *payload, uint32_t len)
{
    uint32_t channel_id;
    uint32_t accepted_count = 0;
    if (sv2_parse_submit_shares_success(payload, len, &channel_id, &accepted_count) == 0) {
        if (m_lastSubmitTimeUs > 0) {
            float response_time_ms = (float)(esp_timer_get_time() - m_lastSubmitTimeUs) / 1000.0f;
            ESP_LOGI(m_tag, "Shares accepted: %lu (%.1f ms)", (unsigned long)accepted_count, response_time_ms);
        } else {
            ESP_LOGI(m_tag, "Shares accepted: %lu", (unsigned long)accepted_count);
        }
        if (accepted_count > 1000) {
            ESP_LOGW(m_tag, "Suspicious accepted_count %lu, capping to 1000", (unsigned long)accepted_count);
            accepted_count = 1000;
        }
        for (uint32_t i = 0; i < accepted_count; i++) {
            m_manager->acceptedShare(m_index);
        }
        m_manager->m_lastSubmitResponseTimestamp = esp_timer_get_time();
    }
}

void StratumTaskV2::handleSubmitSharesError(const uint8_t *payload, uint32_t len)
{
    uint32_t channel_id, seq_num;
    char error_code[64];
    if (sv2_parse_submit_shares_error(payload, len, &channel_id, &seq_num,
                                       error_code, sizeof(error_code)) == 0) {
        ESP_LOGW(m_tag, "Share rejected: %s", error_code);
        m_manager->rejectedShare(m_index);
        m_manager->m_lastSubmitResponseTimestamp = esp_timer_get_time();
    }
}

// ============================================================================
// Share Submission
// ============================================================================

void StratumTaskV2::submitShare(const char *jobid, const char *extranonce_2,
                                const uint32_t ntime, const uint32_t nonce,
                                const uint32_t version_rolled, const uint32_t version_base)
{
    sv2_noise_ctx_t *noise = m_noiseTransport.getNoiseCtx();
    esp_transport_handle_t transport = m_noiseTransport.getTransportHandle();

    if (!noise || !transport) {
        ESP_LOGE(m_tag, "Cannot submit share: no connection");
        return;
    }

    // Convert string job_id to uint32_t (SV2 uses numeric job IDs)
    uint32_t sv2_job_id = (uint32_t)strtoul(jobid, nullptr, 10);

    uint8_t buf[SV2_FRAME_HEADER_SIZE + 24 + 1 + 32]; // max size for extended submit
    int frame_len;

    if (m_channelType == SV2_CHANNEL_EXTENDED && extranonce_2 && strlen(extranonce_2) > 0) {
        // Extended channel: decode hex extranonce2 to binary
        size_t en2_hex_len = strlen(extranonce_2);
        size_t en2_bin_len = en2_hex_len / 2;
        uint8_t en2_bin[32];

        // Simple hex decode
        for (size_t i = 0; i < en2_bin_len && i < sizeof(en2_bin); i++) {
            unsigned int byte;
            sscanf(extranonce_2 + i * 2, "%02x", &byte);
            en2_bin[i] = (uint8_t)byte;
        }

        frame_len = sv2_build_submit_shares_extended(
            buf, sizeof(buf), m_sv2_conn.channel_id,
            m_sv2_conn.sequence_number++,
            sv2_job_id, nonce, ntime, version_rolled,
            en2_bin, (uint8_t)en2_bin_len);
    } else {
        // Standard channel: no extranonce
        frame_len = sv2_build_submit_shares_standard(
            buf, sizeof(buf), m_sv2_conn.channel_id,
            m_sv2_conn.sequence_number++,
            sv2_job_id, nonce, ntime, version_rolled);
    }

    if (frame_len < 0) {
        ESP_LOGE(m_tag, "Failed to build SubmitShares frame");
        return;
    }

    m_lastSubmitTimeUs = esp_timer_get_time();

    if (sv2_noise_send(noise, transport, buf, frame_len) != 0) {
        ESP_LOGE(m_tag, "Failed to send share");
    }
}

// ============================================================================
// Job Delivery (bridge to create_jobs_task via MiningInfo)
// ============================================================================

void StratumTaskV2::enqueueStandardJob(uint32_t job_id, uint32_t version,
                                       const uint8_t merkle_root[32],
                                       const uint8_t prev_hash[32],
                                       uint32_t ntime, uint32_t nbits, bool clean)
{
    uint32_t pdiff = sv2_target_to_pdiff(m_sv2_conn.target);
    create_job_sv2_standard(m_index, job_id, version, merkle_root, prev_hash,
                            ntime, nbits, 0x1fffe000, pdiff, clean);
}

void StratumTaskV2::enqueueExtendedJob(sv2_ext_job_t *job)
{
    uint32_t pdiff = sv2_target_to_pdiff(m_sv2_conn.target);
    create_job_sv2_extended(m_index, job,
                            m_sv2_conn.extranonce_prefix,
                            m_sv2_conn.extranonce_prefix_len,
                            m_sv2_conn.extranonce_size,
                            0x1fffe000, pdiff, job->clean_jobs);
    sv2_ext_job_free(job);
}
