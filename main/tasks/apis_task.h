#pragma once

#include <pthread.h>
#include "ArduinoJson.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

class APIsFetcher {
private:
    const char *TAG = "APIsFetcher";
    static constexpr int BUFFER_SIZE = 1024;

    enum ApiType {
        APItype_PRICE,
        APItype_BLOCK_HEIGHT,
        APItype_HASHRATE,
        APItype_FEES
    };

    uint32_t m_bitcoinPrice;
    uint32_t m_blockHeigh;
    uint64_t m_netHash;
    uint64_t m_netDifficulty;
    uint32_t m_hourFee;
    uint32_t m_halfHourFee;
    uint32_t m_fastestFee;

    char m_responseBuffer[BUFFER_SIZE]; // Buffer to store response
    int m_responseLength;               // Length of the HTTP response

    bool m_enabled = false;          // Flag indicating whether fetching is enabled

    pthread_mutex_t m_mutex;
    pthread_cond_t m_cond;

    // HTTP event handler to process HTTP responses
    static esp_err_t http_event_handler(esp_http_client_event_t *evt);

    bool fetchData(const char* apiUrl, ApiType type);

    // Parses Json Bitcoin price via HTTP request
    bool parseBitcoinPrice(JsonDocument &doc);
    // Parses Json BlockHeight via HTTP request
    bool parseBlockHeight(JsonDocument &doc);
    // Parses Json Hashrate via HTTP request
    bool parseHashrate(JsonDocument &doc);
    // Parses Json Fees via HTTP request
    bool parseFees(JsonDocument &doc);

public:
    APIsFetcher();
    static void taskWrapper(void *pvParameters);

    // Main FreeRTOS task function
    void task();

    // Enables APIs fetching
    void enableFetching();

    // Disables APIs fetching
    void disableFetching();

    // Returns the last fetched Bitcoin price
    uint32_t getPrice();
    uint32_t getBlockHeight();
    uint32_t getBlocksToHalving();
    uint32_t getHalvingPercent();
    uint64_t getNetHash();
    uint64_t getNetDifficulty();
    uint32_t getLowestFee();
    uint32_t getMidFee();
    uint32_t getFastestFee();
};


