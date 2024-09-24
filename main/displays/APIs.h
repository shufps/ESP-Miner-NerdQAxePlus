#pragma once

// Time update period
#define UPDATE_PERIOD_h 5

#define getBTCAPI "http://api.coindesk.com/v1/bpi/currentprice.json"
#define UPDATE_BTC_min 1

unsigned int getBTCprice(void);


