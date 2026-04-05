#include "mining_info_v2.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>

#include "esp_log.h"
#include "mining.h"

extern "C" {
#include "mining_utils.h"
}

static const char *TAG = "mining_info_v2";

// ============================================================================
// MiningInfoV2Standard
// ============================================================================

MiningInfoV2Standard::MiningInfoV2Standard()
{
    memset(m_merkle_root, 0, 32);
    memset(m_prev_hash, 0, 32);
    m_jobid_str[0] = '\0';
}

MiningInfoV2Standard::~MiningInfoV2Standard() {}

void MiningInfoV2Standard::updateJob(uint32_t job_id, uint32_t version,
                                      const uint8_t merkle_root[32],
                                      const uint8_t prev_hash[32],
                                      uint32_t ntime, uint32_t nbits,
                                      uint32_t version_mask, uint32_t difficulty)
{
    m_job_id = job_id;
    m_version = version;
    memcpy(m_merkle_root, merkle_root, 32);
    memcpy(m_prev_hash, prev_hash, 32);
    m_ntime = ntime;
    m_nbits = nbits;
    m_version_mask = version_mask;
    m_difficulty = difficulty;
    m_jobSent = false;  // new job from pool, ready to send
    snprintf(m_jobid_str, sizeof(m_jobid_str), "%lu", (unsigned long)job_id);
}

bm_job *MiningInfoV2Standard::buildBmJob(uint32_t extranonce_2, int pool_id, uint32_t asic_diff)
{
    bm_job *job = (bm_job *)malloc(sizeof(bm_job));
    if (!job) return nullptr;

    job->version = m_version;
    job->version_mask = m_version_mask;
    job->target = m_nbits;
    job->ntime = m_ntime;
    job->starting_nonce = 0;
    job->pool_diff = m_difficulty;
    job->asic_diff = asic_diff;
    job->pool_id = pool_id;

    // SV2 provides merkle_root and prev_hash in internal byte order.
    // NerdQAxePlus test_nonce_value() uses bm_job fields directly (no transform).
    // V1's construct_bm_job applies swap_endian_words_bin to prev_hash and hex2bin
    // to merkle_root. Both results are what test_nonce_value expects.
    //
    // For SV2 data (internal byte order), we need swap_endian_words_bin on BOTH
    // to match what V1 produces and what the ASIC/test_nonce_value expect.
    // The ASIC hardware reads from bm_job->merkle_root_be and prev_block_hash_be
    // (see asic.cpp line 340-341). test_nonce_value reads from merkle_root and prev_block_hash.
    //
    // SV2 provides data in internal byte order (= Bitcoin block header format).
    // The ASIC needs the data in the same format it gets from V1's construct_bm_job.
    //
    // V1 construct_bm_job produces:
    //   merkle_root    = hex2bin(merkle_hex) = raw SHA-256 bytes
    //   merkle_root_be = swap_endian_words(merkle_hex) then reverse_bytes
    //   prev_block_hash    = swap_endian_words_bin(v1_binary)
    //   prev_block_hash_be = v1_binary then reverse_bytes
    //
    // For SV2 (internal byte order = raw SHA-256 bytes):
    //   merkle_root    = direct copy (matches V1's hex2bin result)
    //   merkle_root_be = swap_endian_words_bin then reverse_bytes (matches V1's swap_endian_words+reverse)
    //   prev_block_hash    = direct copy (internal order, same as V1 after its swap)
    //   prev_block_hash_be = reverse_bytes only (no swap needed, SV2 is already unswapped)
    // Normal fields: for test_nonce_value() which does memcpy into header directly
    // _be fields: for ASIC hardware via asic.cpp sendWork()
    //
    // V1 construct_bm_job produces (from hex merkle_root and binary prev_hash):
    //   merkle_root     = hex2bin(hex)
    //   merkle_root_be  = swap_endian_words(hex) + reverse_bytes  [= swap_endian_words_bin(bin) + reverse_bytes]
    //   prev_block_hash = swap_endian_words_bin(v1_bin)
    //   prev_block_hash_be = memcpy(v1_bin) + reverse_bytes       [v1_bin = swap_endian_words_bin(internal)]
    //
    // SV2 data is in internal byte order (= same as hex2bin of V1 hex = raw SHA-256 output).
    // So for SV2:
    //   merkle_root     = memcpy (same as V1's hex2bin)
    //   merkle_root_be  = swap_endian_words_bin + reverse_bytes (same transform as V1)
    //   prev_block_hash = memcpy (internal order = V1 after its swap)
    //   prev_block_hash_be = swap_endian_words_bin + reverse_bytes
    //     (V1 does memcpy(v1_bin)+reverse. v1_bin = swap(internal). So: swap(internal)+reverse = our transform.)
    memcpy(job->merkle_root, m_merkle_root, 32);
    swap_endian_words_bin((uint8_t *)m_merkle_root, job->merkle_root_be, 32);
    reverse_bytes(job->merkle_root_be, 32);

    memcpy(job->prev_block_hash, m_prev_hash, 32);
    swap_endian_words_bin((uint8_t *)m_prev_hash, job->prev_block_hash_be, 32);
    reverse_bytes(job->prev_block_hash_be, 32);

    job->jobid = strdup(m_jobid_str);
    job->extranonce2 = strdup(""); // unused in SV2 standard channel

    // Standard Channel: mark as sent, don't resend on timer
    m_jobSent = true;

    return job;
}

void MiningInfoV2Standard::setDifficulty(uint32_t difficulty)
{
    m_difficulty = difficulty;
    // Do NOT reset m_jobSent. Never resend the same Standard Channel job.
    // ASIC keeps mining with version rolling. New difficulty applies to
    // the NEXT job from the pool (matches Bitaxe behavior).
}

bool MiningInfoV2Standard::isValid() const { return m_ntime != 0 && !m_jobSent; }

bool MiningInfoV2Standard::isNewWork(uint32_t &last_ntime) const
{
    if (last_ntime != m_ntime) {
        last_ntime = m_ntime;
        return true;
    }
    return false;
}

const char *MiningInfoV2Standard::getJobId() const { return m_jobid_str; }
uint32_t MiningInfoV2Standard::getActiveDifficulty() const { return m_difficulty; }
uint32_t MiningInfoV2Standard::getVersionMask() const { return m_version_mask; }

void MiningInfoV2Standard::invalidate()
{
    m_ntime = 0;
    m_job_id = 0;
}

// ============================================================================
// MiningInfoV2Extended
// ============================================================================

MiningInfoV2Extended::MiningInfoV2Extended()
{
    memset(m_prev_hash, 0, 32);
    memset(m_extranonce_prefix, 0, sizeof(m_extranonce_prefix));
    memset(m_merkle_path, 0, sizeof(m_merkle_path));
    m_jobid_str[0] = '\0';
}

MiningInfoV2Extended::~MiningInfoV2Extended()
{
    free(m_coinbase_prefix);
    free(m_coinbase_suffix);
}

void MiningInfoV2Extended::updateJob(const sv2_ext_job_t *ext_job,
                                      const uint8_t *extranonce_prefix,
                                      uint8_t extranonce_prefix_len,
                                      uint8_t extranonce_size,
                                      uint32_t version_mask, uint32_t difficulty)
{
    m_job_id = ext_job->job_id;
    m_version = ext_job->version;
    memcpy(m_prev_hash, ext_job->prev_hash, 32);
    m_ntime = ext_job->ntime;
    m_nbits = ext_job->nbits;
    m_version_mask = version_mask;
    m_difficulty = difficulty;
    snprintf(m_jobid_str, sizeof(m_jobid_str), "%lu", (unsigned long)ext_job->job_id);

    // Copy coinbase components (deep copy)
    free(m_coinbase_prefix);
    m_coinbase_prefix = nullptr;
    if (ext_job->coinbase_prefix && ext_job->coinbase_prefix_len > 0) {
        m_coinbase_prefix = (uint8_t *)malloc(ext_job->coinbase_prefix_len);
        memcpy(m_coinbase_prefix, ext_job->coinbase_prefix, ext_job->coinbase_prefix_len);
        m_coinbase_prefix_len = ext_job->coinbase_prefix_len;
    } else {
        m_coinbase_prefix_len = 0;
    }

    free(m_coinbase_suffix);
    m_coinbase_suffix = nullptr;
    if (ext_job->coinbase_suffix && ext_job->coinbase_suffix_len > 0) {
        m_coinbase_suffix = (uint8_t *)malloc(ext_job->coinbase_suffix_len);
        memcpy(m_coinbase_suffix, ext_job->coinbase_suffix, ext_job->coinbase_suffix_len);
        m_coinbase_suffix_len = ext_job->coinbase_suffix_len;
    } else {
        m_coinbase_suffix_len = 0;
    }

    // Extranonce info
    m_extranonce_prefix_len = extranonce_prefix_len;
    if (extranonce_prefix_len > 0) {
        memcpy(m_extranonce_prefix, extranonce_prefix, extranonce_prefix_len);
    }
    m_extranonce_size = extranonce_size;

    // Merkle path
    m_merkle_path_count = ext_job->merkle_path_count;
    if (m_merkle_path_count > 0 && m_merkle_path_count <= 32) {
        memcpy(m_merkle_path, ext_job->merkle_path, m_merkle_path_count * 32);
    }
}

bm_job *MiningInfoV2Extended::buildBmJob(uint32_t extranonce_2, int pool_id, uint32_t asic_diff)
{
    bm_job *job = (bm_job *)malloc(sizeof(bm_job));
    if (!job) return nullptr;

    // Derive extranonce_2 binary from counter (big-endian)
    uint8_t en2_bin[32];
    memset(en2_bin, 0, sizeof(en2_bin));
    uint32_t counter = extranonce_2;
    for (int i = m_extranonce_size - 1; i >= 0 && counter > 0; i--) {
        en2_bin[i] = (uint8_t)(counter & 0xFF);
        counter >>= 8;
    }

    // Compute coinbase tx hash: double_sha256(prefix + extranonce_prefix + extranonce_2 + suffix)
    size_t coinbase_len = m_coinbase_prefix_len + m_extranonce_prefix_len + m_extranonce_size + m_coinbase_suffix_len;
    uint8_t *coinbase = (uint8_t *)malloc(coinbase_len);
    if (!coinbase) {
        free(job);
        return nullptr;
    }

    size_t pos = 0;
    memcpy(coinbase + pos, m_coinbase_prefix, m_coinbase_prefix_len); pos += m_coinbase_prefix_len;
    memcpy(coinbase + pos, m_extranonce_prefix, m_extranonce_prefix_len); pos += m_extranonce_prefix_len;
    memcpy(coinbase + pos, en2_bin, m_extranonce_size); pos += m_extranonce_size;
    memcpy(coinbase + pos, m_coinbase_suffix, m_coinbase_suffix_len);

    uint8_t coinbase_hash[32];
    double_sha256_bin(coinbase, coinbase_len, coinbase_hash);
    free(coinbase);

    // Compute merkle root from coinbase hash + merkle path
    uint8_t merkle_root[32];
    memcpy(merkle_root, coinbase_hash, 32);
    for (int i = 0; i < m_merkle_path_count; i++) {
        uint8_t concat[64];
        memcpy(concat, merkle_root, 32);
        memcpy(concat + 32, m_merkle_path[i], 32);
        double_sha256_bin(concat, 64, merkle_root);
    }

    // Fill bm_job
    job->version = m_version;
    job->version_mask = m_version_mask;
    job->target = m_nbits;
    job->ntime = m_ntime;
    job->starting_nonce = 0;
    job->pool_diff = m_difficulty;
    job->asic_diff = asic_diff;
    job->pool_id = pool_id;

    // Same byte order handling as Standard channel (see comments above).
    memcpy(job->merkle_root, merkle_root, 32);
    swap_endian_words_bin(merkle_root, job->merkle_root_be, 32);
    reverse_bytes(job->merkle_root_be, 32);

    memcpy(job->prev_block_hash, m_prev_hash, 32);
    swap_endian_words_bin((uint8_t *)m_prev_hash, job->prev_block_hash_be, 32);
    reverse_bytes(job->prev_block_hash_be, 32);

    job->jobid = strdup(m_jobid_str);

    // Store extranonce_2 as hex for share submission
    char en2_hex[65];
    bin2hex(en2_bin, m_extranonce_size, en2_hex, sizeof(en2_hex));
    job->extranonce2 = strdup(en2_hex);

    return job;
}

void MiningInfoV2Extended::setDifficulty(uint32_t difficulty) { m_difficulty = difficulty; }

bool MiningInfoV2Extended::isValid() const { return m_ntime != 0; }

bool MiningInfoV2Extended::isNewWork(uint32_t &last_ntime) const
{
    if (last_ntime != m_ntime) {
        last_ntime = m_ntime;
        return true;
    }
    return false;
}

const char *MiningInfoV2Extended::getJobId() const { return m_jobid_str; }
uint32_t MiningInfoV2Extended::getActiveDifficulty() const { return m_difficulty; }
uint32_t MiningInfoV2Extended::getVersionMask() const { return m_version_mask; }

void MiningInfoV2Extended::invalidate()
{
    m_ntime = 0;
    m_job_id = 0;
    free(m_coinbase_prefix);  m_coinbase_prefix = nullptr;
    free(m_coinbase_suffix);  m_coinbase_suffix = nullptr;
    m_coinbase_prefix_len = 0;
    m_coinbase_suffix_len = 0;
}
