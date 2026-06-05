#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void mdns_service_start(const char *hostname,
                        const char *device_model,
                        const char *asic_model,
                        int asic_count,
                        int board_version);

#ifdef __cplusplus
}
#endif
