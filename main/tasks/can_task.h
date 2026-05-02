#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the TWAI peripheral at 500 kbit/s.
 * TX: GPIO1   RX: GPIO10
 * Call once from app_main before any can_sender_* calls.
 */
void can_init();

#ifdef __cplusplus
}
#endif
