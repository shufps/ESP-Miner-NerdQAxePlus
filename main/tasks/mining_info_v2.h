#pragma once

#include "create_jobs_task.h"
#include "mining.h"

extern "C" {
#include "sv2_protocol.h"
}

/**
 * @brief MiningInfo for SV2 Standard Channel.
 *
 * Pool provides complete merkle_root and prev_hash - no coinbase computation needed.
 * bm_job is filled directly from pool-provided data.
 */
class MiningInfoV2Standard : public MiningInfoBase {
  public:
    MiningInfoV2Standard();
    ~MiningInfoV2Standard() override;

    bm_job *buildBmJob(uint32_t extranonce_2, int pool_id, uint32_t asic_diff) override;
    bool isValid() const override;
    bool isNewWork(uint32_t &last_ntime) const override;
    const char *getJobId() const override;
    uint32_t getActiveDifficulty() const override;
    uint32_t getVersionMask() const override;
    void invalidate() override;

    /// Update difficulty mid-job (from SetTarget)
    void setDifficulty(uint32_t difficulty);

    /// Update with new job data from SV2 pool
    void updateJob(uint32_t job_id, uint32_t version,
                   const uint8_t merkle_root[32], const uint8_t prev_hash[32],
                   uint32_t ntime, uint32_t nbits,
                   uint32_t version_mask, uint32_t difficulty);

  private:
    uint32_t m_job_id = 0;
    uint32_t m_version = 0;
    uint8_t m_merkle_root[32];
    uint8_t m_prev_hash[32];
    uint32_t m_ntime = 0;
    uint32_t m_nbits = 0;
    uint32_t m_version_mask = 0x1fffe000;
    uint32_t m_difficulty = 0;
    char m_jobid_str[16];
    bool m_jobSent = false;  ///< Standard Channel: job already sent to ASIC, don't resend
};


/**
 * @brief MiningInfo for SV2 Extended Channel.
 *
 * Pool provides coinbase prefix/suffix and merkle path.
 * Miner computes coinbase from prefix + extranonce_prefix + extranonce_2 + suffix,
 * then merkle root from the merkle path.
 */
class MiningInfoV2Extended : public MiningInfoBase {
  public:
    MiningInfoV2Extended();
    ~MiningInfoV2Extended() override;

    bm_job *buildBmJob(uint32_t extranonce_2, int pool_id, uint32_t asic_diff) override;
    bool isValid() const override;
    bool isNewWork(uint32_t &last_ntime) const override;
    const char *getJobId() const override;
    uint32_t getActiveDifficulty() const override;
    uint32_t getVersionMask() const override;
    void invalidate() override;

    /// Update difficulty mid-job (from SetTarget)
    void setDifficulty(uint32_t difficulty);

    /// Update with new extended job data from SV2 pool
    void updateJob(const sv2_ext_job_t *job,
                   const uint8_t *extranonce_prefix, uint8_t extranonce_prefix_len,
                   uint8_t extranonce_size,
                   uint32_t version_mask, uint32_t difficulty);

  private:
    uint32_t m_job_id = 0;
    uint32_t m_version = 0;
    uint8_t m_prev_hash[32];
    uint32_t m_ntime = 0;
    uint32_t m_nbits = 0;
    uint32_t m_version_mask = 0x1fffe000;
    uint32_t m_difficulty = 0;
    char m_jobid_str[16];

    // Coinbase components (owned copies)
    uint8_t *m_coinbase_prefix = nullptr;
    uint16_t m_coinbase_prefix_len = 0;
    uint8_t *m_coinbase_suffix = nullptr;
    uint16_t m_coinbase_suffix_len = 0;

    // Extranonce from pool
    uint8_t m_extranonce_prefix[32];
    uint8_t m_extranonce_prefix_len = 0;
    uint8_t m_extranonce_size = 0;  // miner's rollable portion

    // Merkle path
    uint8_t m_merkle_path[32][32];
    int m_merkle_path_count = 0;
};
