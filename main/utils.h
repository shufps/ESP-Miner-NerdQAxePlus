#pragma once
#include <stdint.h>

void suffixString(uint64_t val, char* buf, size_t bufSize, int sigDigits);
double calculateNetworkDifficulty(uint32_t nBits);