#pragma once

#include "influx.h"

void influx_task_set_temperature(float temp, float temp2);
void influx_task_set_hashrate(float hashrate);

void * influx_task(void * pvParameters);