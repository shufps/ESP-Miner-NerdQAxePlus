#pragma once

#include "esp_http_client.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class APIsFetcher {
private:
    const char *TAG = "APIsFetcher";
    static constexpr int BUFFER_SIZE = 256;

    uint32_t m_bitcoinPrice;
    uint32_t m_blockHeigh;

    char m_responseBuffer[BUFFER_SIZE]; // Buffer to store response
    int m_responseLength;

    static esp_err_t http_event_handler(esp_http_client_event_t *evt);

    bool fetchBitcoinPrice();
    bool fetchBlockHeight();

public:
    APIsFetcher();
    static void taskWrapper(void *pvParameters);
    void task();

    uint32_t getPrice();
};


