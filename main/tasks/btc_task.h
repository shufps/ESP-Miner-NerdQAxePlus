#pragma once

#include <pthread.h>

#include "esp_err.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

class BitcoinPriceFetcher {
  private:
    const char *TAG = "BitcoinFetcher";     // Logging tag
    static constexpr int BUFFER_SIZE = 256; // Response buffer size

    uint32_t m_bitcoinPrice; // Last fetched Bitcoin price

    char m_responseBuffer[BUFFER_SIZE]; // Buffer to store response
    int m_responseLength;               // Length of the HTTP response

    bool m_enabled = false;          // Flag indicating whether fetching is enabled

    pthread_mutex_t m_mutex;
    pthread_cond_t m_cond;

    // HTTP event handler to process HTTP responses
    static esp_err_t http_event_handler(esp_http_client_event_t *evt);

    // Fetches Bitcoin price via HTTP request
    bool fetchBitcoinPrice();
    bool parseBitcoinPrice(const char *jsonData);

  public:
    // Constructor - Initializes member variables and event group
    BitcoinPriceFetcher();

    // Wrapper function to launch the task
    static void taskWrapper(void *pvParameters);

    // Main FreeRTOS task function
    void task();

    // Enables Bitcoin price fetching
    void enableFetching();

    // Disables Bitcoin price fetching
    void disableFetching();

    // Returns the last fetched Bitcoin price
    uint32_t getPrice();
};