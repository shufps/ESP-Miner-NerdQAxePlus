#pragma once

#include "esp_http_client.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class BitcoinPriceFetcher {
private:
    const char *TAG = "BitcoinFetcher";
    static constexpr int BUFFER_SIZE = 256;

    uint32_t m_bitcoinPrice;

    char m_responseBuffer[BUFFER_SIZE]; // Buffer to store response
    int m_responseLength;

    static esp_err_t http_event_handler(esp_http_client_event_t *evt);

    bool fetchBitcoinPrice();

public:
    BitcoinPriceFetcher();
    static void taskWrapper(void *pvParameters);
    void task();

    uint32_t getPrice();
};


