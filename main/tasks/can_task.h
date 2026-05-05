#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the TWAI peripheral at 500 kbit/s.
 * Call once from app_main before any can_sender_* calls.
 */
void can_init(int tx_gpio, int rx_gpio);

#ifdef __cplusplus
}
#endif
