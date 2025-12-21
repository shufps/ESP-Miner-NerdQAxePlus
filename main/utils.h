#pragma once
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void suffixString(uint64_t val, char* buf, size_t bufSize, int sigDigits);
double calculateNetworkDifficulty(uint32_t nBits);

BaseType_t xTaskCreatePSRAM(TaskFunction_t pxTaskCode, const char *const pcName, const uint32_t usStackDepthBytes,
                            void *const pvParameters, UBaseType_t uxPriority, TaskHandle_t *const pxCreatedTask);