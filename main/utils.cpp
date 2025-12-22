#include "utils.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"

void suffixString(uint64_t val, char *buf, size_t bufSize, int sigDigits)
{
    const double kKilo = 1000.0;
    const uint64_t kKiloUll = 1000ull;
    const uint64_t kMegaUll = 1000000ull;
    const uint64_t kGigaUll = 1000000000ull;
    const uint64_t kTeraUll = 1000000000000ull;
    const uint64_t kPetaUll = 1000000000000000ull;
    const uint64_t kExaUll = 1000000000000000000ull;
    char suffix[2] = "";
    bool decimal = true;
    double dval;

    if (val >= kExaUll) {
        val /= kPetaUll;
        dval = (double) val / kKilo;
        strcpy(suffix, "E");
    } else if (val >= kPetaUll) {
        val /= kTeraUll;
        dval = (double) val / kKilo;
        strcpy(suffix, "P");
    } else if (val >= kTeraUll) {
        val /= kGigaUll;
        dval = (double) val / kKilo;
        strcpy(suffix, "T");
    } else if (val >= kGigaUll) {
        val /= kMegaUll;
        dval = (double) val / kKilo;
        strcpy(suffix, "G");
    } else if (val >= kMegaUll) {
        val /= kKiloUll;
        dval = (double) val / kKilo;
        strcpy(suffix, "M");
    } else if (val >= kKiloUll) {
        dval = (double) val / kKilo;
        strcpy(suffix, "k");
    } else {
        dval = val;
        decimal = false;
    }

    if (!sigDigits) {
        if (decimal)
            snprintf(buf, bufSize, "%.3g%s", dval, suffix);
        else
            snprintf(buf, bufSize, "%d%s", (unsigned int) dval, suffix);
    } else {
        int nDigits = sigDigits - 1 - (dval > 0.0 ? floor(log10(dval)) : 0);
        // snprintf(buf, bufSize, "%*.*f%s", sigDigits + 1, nDigits, dval, suffix);
        if (nDigits < 0)
            nDigits = 0;
        snprintf(buf, bufSize, "%.*f%s", nDigits, dval, suffix);
    }
}

double calculateNetworkDifficulty(uint32_t nBits)
{
    uint32_t mantissa = nBits & 0x007fffff;  // Extract the mantissa from nBits
    uint8_t exponent = (nBits >> 24) & 0xff; // Extract the exponent from nBits

    double target = (double) mantissa * pow(256, (exponent - 3)); // Calculate the target value
    double difficulty = (pow(2, 208) * 65535) / target;           // Calculate the difficulty

    return difficulty;
}

BaseType_t xTaskCreatePSRAM(TaskFunction_t pxTaskCode, const char *const pcName, const uint32_t usStackDepthBytes,
                            void *const pvParameters, UBaseType_t uxPriority, TaskHandle_t *const pxCreatedTask)
{
    const uint32_t stackDepth = usStackDepthBytes / sizeof(StackType_t);

    // put the stack on the psram
    StackType_t *stack = (StackType_t *) heap_caps_malloc(stackDepth * sizeof(StackType_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    // don't put the TCB into psram
    StaticTask_t *tcb = (StaticTask_t *) heap_caps_malloc(sizeof(StaticTask_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    if (!stack || !tcb) {
        ESP_LOGE("task_psram", "PSRAM task alloc failed (stack=%p tcb=%p)", stack, tcb);
        if (stack)
            heap_caps_free(stack);
        if (tcb)
            heap_caps_free(tcb);
        return errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
    }

    TaskHandle_t handle = xTaskCreateStatic(pxTaskCode, pcName, stackDepth, pvParameters, uxPriority, stack, tcb);

    if (!handle) {
        ESP_LOGE("task_psram", "xTaskCreateStatic failed");
        heap_caps_free(stack);
        heap_caps_free(tcb);
        return errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
    }

    if (pxCreatedTask) {
        *pxCreatedTask = handle;
    }

    ESP_LOGI("task_psram", "Task '%s' created in PSRAM (stack=%lu bytes)", pcName, usStackDepthBytes);

    return pdPASS;
}
