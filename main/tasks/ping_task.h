#pragma once

#include <stdint.h>

void ping_task(void *pvParameters);
double get_last_ping_rtt();
double get_recent_ping_loss();
