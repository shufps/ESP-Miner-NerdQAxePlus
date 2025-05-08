#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void ping_task(void *pvParameters);
double get_last_ping_rtt();

#ifdef __cplusplus
}
#endif
