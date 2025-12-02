#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

int hex2char(uint8_t x, char *c);

size_t bin2hex(const uint8_t *buf, size_t buflen, char *hex, size_t hexlen);

uint8_t hex2val(char c);
void flip80bytes(void *dest_p, const void *src_p);
void flip32bytes(void *dest_p, const void *src_p);

size_t hex2bin(const char *hex, uint8_t *bin, size_t bin_len);

void print_hex(const uint8_t *b, size_t len, const size_t in_line, const char *prefix);

void double_sha256(const char *hex_string, uint8_t output_hash[65]);

void double_sha256_bin(const uint8_t *data, const size_t data_len, uint8_t hash[32]);

void single_sha256_bin(const uint8_t *data, const size_t data_len, uint8_t *dest);

void swap_endian_words(const char *hex, uint8_t *output);
void swap_endian_words_bin(uint8_t *data, uint8_t *output, size_t data_length);

void reverse_bytes(uint8_t *data, size_t len);

double le256todouble(const void *target);

void prettyHex(unsigned char *buf, int len);

uint32_t flip32(uint32_t val);

unsigned char _reverse_bits(unsigned char num);

int _largest_power_of_two(int num);

#ifdef __cplusplus
}
#endif
