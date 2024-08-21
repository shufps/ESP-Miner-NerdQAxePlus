#include "mining.h"
#include "mbedtls/sha256.h"
#include "utils.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

#ifdef DEBUG_MEMORY_LOGGING
#include "leak_tracker.h"
#endif

void free_bm_job(bm_job *job)
{
    free(job->jobid);
    free(job->extranonce2);
    free(job);
}

void calculate_merkle_root_hash(const char *coinbase_tx, const uint8_t merkle_branches[][32], const int num_merkle_branches, char merkle_root_hash[65])
{
    size_t coinbase_tx_bin_len = strlen(coinbase_tx) / 2;
    uint8_t coinbase_tx_bin[coinbase_tx_bin_len];

    hex2bin(coinbase_tx, coinbase_tx_bin, coinbase_tx_bin_len);

    uint8_t both_merkles[64];
    uint8_t new_root[32];
    double_sha256_bin(coinbase_tx_bin, coinbase_tx_bin_len, new_root);

    memcpy(both_merkles, new_root, 32);
    for (int i = 0; i < num_merkle_branches; i++) {
        memcpy(both_merkles + 32, merkle_branches[i], 32);
        double_sha256_bin(both_merkles, 64, new_root);
        memcpy(both_merkles, new_root, 32);
    }

    bin2hex(both_merkles, 32, merkle_root_hash, 65);
}

// take a mining_notify struct with ascii hex strings and convert it to a bm_job struct
void construct_bm_job(mining_notify *params, const char *merkle_root, const uint32_t version_mask, bm_job *new_job)
{
    new_job->version = params->version;
    new_job->starting_nonce = 0;
    new_job->target = params->target;
    new_job->ntime = params->ntime;
    new_job->pool_diff = params->difficulty;

    hex2bin(merkle_root, new_job->merkle_root, 32);

    // hex2bin(merkle_root, new_job.merkle_root_be, 32);
    swap_endian_words(merkle_root, new_job->merkle_root_be);
    reverse_bytes(new_job->merkle_root_be, 32);

    swap_endian_words_bin(params->_prev_block_hash, new_job->prev_block_hash, HASH_SIZE);
    memcpy(new_job->prev_block_hash_be, params->_prev_block_hash, HASH_SIZE);

    // hex2bin(params->prev_block_hash, new_job.prev_block_hash_be, 32);
    reverse_bytes(new_job->prev_block_hash_be, 32);
}

///////cgminer nonce testing
/* truediffone == 0x00000000FFFF0000000000000000000000000000000000000000000000000000
 */
static const double truediffone = 26959535291011309493156476344723991336010898738574164086137773096960.0;

/* testing a nonce and return the diff - 0 means invalid */
double test_nonce_value(const bm_job *job, const uint32_t nonce, const uint32_t rolled_version)
{
    double d64, s64, ds;
    unsigned char header[80];

    // copy data from job to header
    memcpy(header, &rolled_version, 4);
    memcpy(header + 4, job->prev_block_hash, 32);
    memcpy(header + 36, job->merkle_root, 32);
    memcpy(header + 68, &job->ntime, 4);
    memcpy(header + 72, &job->target, 4);
    memcpy(header + 76, &nonce, 4);

    unsigned char hash_buffer[32];
    unsigned char hash_result[32];

    // double hash the header
    mbedtls_sha256(header, 80, hash_buffer, 0);
    mbedtls_sha256(hash_buffer, 32, hash_result, 0);

    d64 = truediffone;
    s64 = le256todouble(hash_result);
    ds = d64 / s64;

    return ds;
}

