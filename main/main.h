#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include "connect.h"

    void MINER_set_wifi_status(wifi_status_t status, uint16_t retry_count);

#ifdef __cplusplus
}
#endif

void self_test(void *pvParameters);
