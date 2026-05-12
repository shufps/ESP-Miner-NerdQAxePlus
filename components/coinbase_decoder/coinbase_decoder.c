/*
 * Coinbase transaction decoder for Bitcoin mining firmware.
 */

#include "coinbase_decoder.h"
#include "mining_utils.h"
#include "segwit_addr.h"
#include "libbase58.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "mbedtls/sha256.h"

#define MAX_ADDRESS_STRING_LEN 128

/* Network difficulty calculation from nBits (same as calculateNetworkDifficulty) */
static double calc_network_difficulty(uint32_t nbits)
{
    uint32_t mantissa = nbits & 0x007fffff;
    uint8_t exponent = (nbits >> 24) & 0xff;

    if (exponent <= 3) {
        mantissa >>= 8 * (3 - exponent);
        if (mantissa == 0) return 0.0;
        return (double) 0xFFFF / (double) mantissa;
    }

    if (mantissa == 0) return 0.0;
    return (double) 0xFFFF / (double) mantissa * (1ULL << (8 * (exponent - 3))) / (1ULL << 16);
}

/* SHA256 wrapper for libbase58 */
static bool sha256_for_base58(void *digest, const void *data, size_t datasz)
{
    mbedtls_sha256(data, datasz, digest, 0);
    return true;
}

static void ensure_base58_init(void)
{
    if (b58_sha256_impl == NULL) {
        b58_sha256_impl = sha256_for_base58;
    }
}

/* Decode a Bitcoin varint from binary data. Returns 0 and does not advance
 * offset if there are not enough bytes remaining. */
static uint64_t decode_varint(const uint8_t *data, int *offset, int data_len)
{
    if (*offset >= data_len) return 0;

    uint8_t first_byte = data[*offset];
    (*offset)++;

    if (first_byte < 0xFD) {
        return first_byte;
    } else if (first_byte == 0xFD) {
        if (*offset + 2 > data_len) return 0;
        uint64_t value = data[*offset] | (data[*offset + 1] << 8);
        *offset += 2;
        return value;
    } else if (first_byte == 0xFE) {
        if (*offset + 4 > data_len) return 0;
        uint64_t value = data[*offset] | (data[*offset + 1] << 8) |
                        (data[*offset + 2] << 16) | (data[*offset + 3] << 24);
        *offset += 4;
        return value;
    } else {
        if (*offset + 8 > data_len) return 0;
        uint64_t value = 0;
        for (int i = 0; i < 8; i++) {
            value |= ((uint64_t)data[*offset + i]) << (i * 8);
        }
        *offset += 8;
        return value;
    }
}

/* Decode Bitcoin address from scriptPubKey */
static void decode_address(const uint8_t *script, size_t script_len,
                           char *output, size_t output_len)
{
    if (script_len == 0 || output_len < 65) {
        snprintf(output, output_len, "unknown");
        return;
    }

    ensure_base58_init();

    /* P2PKH: OP_DUP OP_HASH160 <20 bytes> OP_EQUALVERIFY OP_CHECKSIG */
    if (script_len == 25 && script[0] == OP_DUP && script[1] == OP_HASH160 &&
        script[2] == OP_PUSHDATA_20 && script[23] == OP_EQUALVERIFY && script[24] == OP_CHECKSIG) {
        size_t b58sz = output_len;
        if (b58check_enc(output, &b58sz, 0x00, script + 3, 20)) {
            return;
        }
        snprintf(output, output_len, "P2PKH:");
        bin2hex(script + 3, 20, output + 6, output_len - 6);
        return;
    }

    /* P2SH: OP_HASH160 <20 bytes> OP_EQUAL */
    if (script_len == 23 && script[0] == OP_HASH160 && script[1] == OP_PUSHDATA_20 && script[22] == OP_EQUAL) {
        size_t b58sz = output_len;
        if (b58check_enc(output, &b58sz, 0x05, script + 2, 20)) {
            return;
        }
        snprintf(output, output_len, "P2SH:");
        bin2hex(script + 2, 20, output + 5, output_len - 5);
        return;
    }

    /* P2WPKH: OP_0 <20 bytes> */
    if (script_len == 22 && script[0] == OP_0 && script[1] == OP_PUSHDATA_20) {
        if (segwit_addr_encode(output, "bc", 0, script + 2, 20)) {
            return;
        }
        snprintf(output, output_len, "P2WPKH:");
        bin2hex(script + 2, 20, output + 7, output_len - 7);
        return;
    }

    /* P2WSH: OP_0 <32 bytes> */
    if (script_len == 34 && script[0] == OP_0 && script[1] == OP_PUSHDATA_32) {
        if (segwit_addr_encode(output, "bc", 0, script + 2, 32)) {
            return;
        }
        snprintf(output, output_len, "P2WSH:");
        bin2hex(script + 2, 32, output + 6, output_len - 6);
        return;
    }

    /* P2TR: OP_1 <32 bytes> */
    if (script_len == 34 && script[0] == OP_1 && script[1] == OP_PUSHDATA_32) {
        if (segwit_addr_encode(output, "bc", 1, script + 2, 32)) {
            return;
        }
        snprintf(output, output_len, "P2TR:");
        bin2hex(script + 2, 32, output + 5, output_len - 5);
        return;
    }

    /* OP_RETURN: null data output */
    if (script_len > 0 && script[0] == OP_RETURN) {
        snprintf(output, output_len, "OP_RETURN: ");
        size_t offset = 1;

        if (script_len > 1 && script[1] > 0 && script[1] <= 0x4b && (size_t)script[1] + 2 == script_len) {
            offset = 2;
        }

        size_t out_idx = strlen(output);
        for (size_t i = offset; i < script_len && out_idx < output_len - 1; i++) {
            unsigned char c = script[i];
            output[out_idx++] = isprint(c) ? c : '.';
        }
        output[out_idx] = '\0';
        return;
    }

    /* Unknown format */
    snprintf(output, output_len, "UNKNOWN:");
    size_t hex_len = script_len < 32 ? script_len : 32;
    bin2hex(script, hex_len, output + 8, output_len - 8);
}

esp_err_t coinbase_process(const char *coinbase_1,
                           const char *coinbase_2,
                           uint32_t block_version,
                           uint32_t nbits,
                           const char *extranonce1,
                           int extranonce2_len,
                           const char *user_address,
                           coinbase_result_t *result)
{
    if (!coinbase_1 || !coinbase_2 || !extranonce1 || !result) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Initialize result */
    memset(result, 0, sizeof(coinbase_result_t));

    /* 1. Calculate difficulty */
    result->network_difficulty = calc_network_difficulty(nbits);

    /* 2. Parse coinbase_1 for block height and scriptsig */
    int coinbase_1_len = strlen(coinbase_1) / 2;
    int coinbase_1_offset = 41; /* Skip: version(4) + inputcount(1) + prevhash(32) + vout(4) */

    if (coinbase_1_len < coinbase_1_offset) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t scriptsig_len;
    hex2bin(coinbase_1 + (coinbase_1_offset * 2), &scriptsig_len, 1);
    coinbase_1_offset++;

    if (coinbase_1_len < coinbase_1_offset) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t block_height_len;
    hex2bin(coinbase_1 + (coinbase_1_offset * 2), &block_height_len, 1);
    coinbase_1_offset++;

    if (coinbase_1_len < coinbase_1_offset || block_height_len == 0 || block_height_len > 4) {
        return ESP_ERR_INVALID_ARG;
    }

    result->block_height = 0;
    hex2bin(coinbase_1 + (coinbase_1_offset * 2), (uint8_t *)&result->block_height, block_height_len);
    coinbase_1_offset += block_height_len;

    /* Detect BIP-110 signaling: version bit 4 set */
    result->bip110_signaling =
        result->block_height < BIP110_SIGNAL_EXPIRY_BLOCK &&
        (block_version & (1U << BIP110_SIGNAL_BIT)) != 0;

    /* Calculate remaining scriptsig length (excluding block height part) */
    if (scriptsig_len < 1 + block_height_len) {
        return ESP_ERR_INVALID_ARG;
    }
    int scriptsig_length = scriptsig_len - 1 - block_height_len;
    size_t extranonce1_len = strlen(extranonce1) / 2;

    /* Check if scriptsig extends into coinbase_2 (covers extranonces) */
    if (coinbase_1_len - coinbase_1_offset < scriptsig_length) {
        scriptsig_length -= (extranonce1_len + extranonce2_len);
    }

    /* Extract miner tag (scriptsig) */
    if (scriptsig_length > 0 && scriptsig_length < MAX_SCRIPTSIG_LEN) {
        int coinbase_1_tag_len = coinbase_1_len - coinbase_1_offset;
        if (coinbase_1_tag_len > scriptsig_length) {
            coinbase_1_tag_len = scriptsig_length;
        }

        hex2bin(coinbase_1 + (coinbase_1_offset * 2), (uint8_t *)result->scriptsig, coinbase_1_tag_len);

        int coinbase_2_tag_len = scriptsig_length - coinbase_1_tag_len;
        int coinbase_2_len = strlen(coinbase_2) / 2;

        if (coinbase_2_len >= coinbase_2_tag_len && coinbase_2_tag_len > 0) {
            hex2bin(coinbase_2, (uint8_t *)result->scriptsig + coinbase_1_tag_len, coinbase_2_tag_len);
        }

        /* Filter non-printable characters */
        for (int i = 0; i < scriptsig_length; i++) {
            if (!isprint((unsigned char)result->scriptsig[i])) {
                result->scriptsig[i] = '.';
            }
        }
        result->scriptsig[scriptsig_length] = '\0';
    }

    /* 3. Parse coinbase_2 for outputs */
    int raw_scriptsig_remainder = (scriptsig_len - 1 - block_height_len) - (coinbase_1_len - coinbase_1_offset);

    int coinbase_2_offset = 0;
    if (raw_scriptsig_remainder > 0) {
        int remainder_in_coinbase_2 = raw_scriptsig_remainder - (extranonce1_len + extranonce2_len);
        if (remainder_in_coinbase_2 > 0) {
            coinbase_2_offset = remainder_in_coinbase_2;
        }
    }

    int coinbase_2_len = strlen(coinbase_2) / 2;
    uint8_t *coinbase_2_bin = malloc(coinbase_2_len);
    if (!coinbase_2_bin) {
        return ESP_ERR_NO_MEM;
    }

    hex2bin(coinbase_2, coinbase_2_bin, coinbase_2_len);

    int offset = coinbase_2_offset;

    /* Read nSequence (4 bytes) for BIP-54 detection */
    if (offset + 4 > coinbase_2_len) {
        free(coinbase_2_bin);
        return ESP_OK; /* partial success: got block height + scriptsig */
    }
    uint32_t nSequence = 0;
    for (int i = 0; i < 4; i++) {
        nSequence |= ((uint32_t)coinbase_2_bin[offset + i]) << (i * 8);
    }
    offset += 4;

    /* Decode output count */
    if (offset >= coinbase_2_len) {
        free(coinbase_2_bin);
        return ESP_OK;
    }

    uint64_t num_outputs = decode_varint(coinbase_2_bin, &offset, coinbase_2_len);

    /* Parse each output: accumulate total value and match user address */
    for (uint64_t i = 0; i < num_outputs && offset < coinbase_2_len; i++) {
        /* Read value (8 bytes, little-endian) */
        if (offset + 8 > coinbase_2_len) break;

        uint64_t value_satoshis = 0;
        for (int j = 0; j < 8; j++) {
            value_satoshis |= ((uint64_t)coinbase_2_bin[offset + j]) << (j * 8);
        }
        offset += 8;

        result->total_value_satoshis += value_satoshis;

        /* Read scriptPubKey length */
        if (offset >= coinbase_2_len) break;
        uint64_t script_len = decode_varint(coinbase_2_bin, &offset, coinbase_2_len);

        if (offset + script_len > (size_t)coinbase_2_len) break;

        /* Match user address for payout share calculation */
        if (user_address && value_satoshis > 0) {
            char output_address[MAX_ADDRESS_STRING_LEN];
            decode_address(coinbase_2_bin + offset, script_len, output_address, MAX_ADDRESS_STRING_LEN);
            if (strcmp(user_address, output_address) == 0) {
                result->user_value_satoshis += value_satoshis;
            }
        }

        offset += script_len;
    }

    /* Read nLockTime (4 bytes) for BIP-54 detection */
    uint32_t nLockTime = 0;
    if (offset + 4 <= coinbase_2_len) {
        for (int i = 0; i < 4; i++) {
            nLockTime |= ((uint32_t)coinbase_2_bin[offset + i]) << (i * 8);
        }
    }

    result->bip54_signaling =
        result->block_height > 0 &&
        (nLockTime == result->block_height - 1) && (nSequence != 0xffffffff);

    free(coinbase_2_bin);
    return ESP_OK;
}
