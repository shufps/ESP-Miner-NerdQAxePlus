#pragma once

#include <stdint.h>

extern "C" {
#include "sv2_protocol.h"
}

/**
 * @brief SV2 job delivery functions.
 *
 * Called from StratumTaskV2 when complete jobs are ready.
 * These replace/update miningInfo[pool] with the appropriate
 * MiningInfoV2 instance and trigger job creation.
 */

/// Deliver a standard channel job (pool-provided merkle root + prev hash)
void create_job_sv2_standard(int pool, uint32_t job_id, uint32_t version,
                              const uint8_t merkle_root[32], const uint8_t prev_hash[32],
                              uint32_t ntime, uint32_t nbits,
                              uint32_t version_mask, uint32_t difficulty,
                              bool clean);

/// Deliver an extended channel job (coinbase prefix/suffix + merkle path)
void create_job_sv2_extended(int pool, const sv2_ext_job_t *job,
                              const uint8_t *extranonce_prefix, uint8_t extranonce_prefix_len,
                              uint8_t extranonce_size,
                              uint32_t version_mask, uint32_t difficulty,
                              bool clean);

/// Update SV2 pool difficulty
void create_job_sv2_set_difficulty(int pool, uint32_t difficulty);
