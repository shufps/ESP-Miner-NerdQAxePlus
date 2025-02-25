#pragma once

#include "esp_http_client.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class APIsFetcher {
private:
    const char *TAG = "APIsFetcher";
    static constexpr int BUFFER_SIZE = 1024;

    uint32_t m_bitcoinPrice;
    uint32_t m_blockHeigh;
    uint64_t m_netHash;
    uint64_t m_netDifficulty;

    char m_responseBuffer[BUFFER_SIZE]; // Buffer to store response
    int m_responseLength;

    static esp_err_t http_event_handler(esp_http_client_event_t *evt);

    bool fetchBitcoinPrice();
    bool fetchBlockHeight();
    bool fetchNetHash();

public:
    APIsFetcher();
    static void taskWrapper(void *pvParameters);
    void task();

    uint32_t getPrice();
    uint32_t getBlockHeight();
    uint64_t getNetHash();
    uint64_t getNetDifficulty();
};


