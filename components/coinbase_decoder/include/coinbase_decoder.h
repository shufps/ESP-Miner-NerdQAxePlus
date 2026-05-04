/*
 * Coinbase transaction decoder for Bitcoin mining firmware.
 * Extracts block height, scriptsig (miner tag), and transaction outputs
 * from stratum mining.notify coinbase data.
 */

#ifndef COINBASE_DECODER_H
#define COINBASE_DECODER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_SCRIPTSIG_LEN 256

/* Bitcoin Script Opcodes */
#define OP_0            0x00
#define OP_PUSHDATA_20  0x14
#define OP_PUSHDATA_32  0x20
#define OP_1            0x51
#define OP_RETURN       0x6a
#define OP_DUP          0x76
#define OP_EQUAL        0x87
#define OP_EQUALVERIFY  0x88
#define OP_HASH160      0xa9
#define OP_CHECKSIG     0xac

/* BIP-110 signaling: version bit 4 */
#define BIP110_SIGNAL_BIT 4
#define BIP110_SIGNAL_EXPIRY_BLOCK 965664

/**
 * Result of processing a mining notification's coinbase data.
 */
typedef struct {
    double network_difficulty;
    uint32_t block_height;
    char scriptsig[MAX_SCRIPTSIG_LEN];
    uint64_t total_value_satoshis;
    uint64_t user_value_satoshis;
    bool bip54_signaling;
    bool bip110_signaling;
} coinbase_result_t;

/**
 * Process coinbase data from a mining.notify notification.
 *
 * This is a self-contained C function that takes the raw coinbase fields
 * from stratum, without depending on the C++ stratum_api.h header.
 *
 * @param coinbase_1        Hex string of coinbase part 1 (before extranonces)
 * @param coinbase_2        Hex string of coinbase part 2 (after extranonces)
 * @param block_version     Block version from mining.notify
 * @param nbits             nBits/target from mining.notify
 * @param extranonce1       Hex string of extranonce1
 * @param extranonce2_len   Length of extranonce2 in bytes
 * @param user_address      User's payout address (for output matching), may be NULL
 * @param decode_coinbase_tx Enable full coinbase TX decoding (addresses, values)
 * @param result            Output: decoded result
 * @return ESP_OK on success
 */
esp_err_t coinbase_process(const char *coinbase_1,
                           const char *coinbase_2,
                           uint32_t block_version,
                           uint32_t nbits,
                           const char *extranonce1,
                           int extranonce2_len,
                           const char *user_address,
                           coinbase_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* COINBASE_DECODER_H */
