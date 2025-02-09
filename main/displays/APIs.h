#pragma once

// Time update period
#define UPDATE_PERIOD_h 5

#define getBTCAPI "https://mempool.space/api/v1/prices"
#define UPDATE_BTC_min 1

unsigned int getBTCprice(void);
