#include "btc_task.h"
#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/event_groups.h"
#include <cstring>

static const char *TAG = "BitcoinFetcher";
#define getBTCAPI "https://mempool.space/api/v1/prices"
#define MIN(a, b) ((a) < (b)) ? (a) : (b)

#define FETCH_EVENT_BIT (1 << 0)

// Constructor - Initializes the fetcher
BitcoinPriceFetcher::BitcoinPriceFetcher()
{
    m_bitcoinPrice = 0;
    m_responseLength = 0;
    m_enabled = false;

    // Initialize mutex and condition variable
    pthread_mutex_init(&m_mutex, nullptr);
    pthread_cond_init(&m_cond, nullptr);
}

// Enable fetching - Wakes up the fetcher thread immediately
void BitcoinPriceFetcher::enableFetching()
{
    pthread_mutex_lock(&m_mutex);
    m_enabled = true;
    pthread_cond_signal(&m_cond); // Wake up thread immediately
    pthread_mutex_unlock(&m_mutex);
}

// Disable fetching - Stops the fetching process
void BitcoinPriceFetcher::disableFetching()
{
    m_enabled = false;
}

// Get latest Bitcoin price - Returns the last fetched price
uint32_t BitcoinPriceFetcher::getPrice()
{
    return m_bitcoinPrice;
}

// HTTP event handler - Processes received HTTP data
esp_err_t BitcoinPriceFetcher::http_event_handler(esp_http_client_event_t *evt)
{
    BitcoinPriceFetcher *instance = static_cast<BitcoinPriceFetcher *>(evt->user_data);

    switch (evt->event_id) {
    case HTTP_EVENT_ON_DATA:
        if (!esp_http_client_is_chunked_response(evt->client)) {
            int copyLength = MIN(evt->data_len, instance->BUFFER_SIZE - instance->m_responseLength - 1);
            if (copyLength > 0) {
                memcpy(instance->m_responseBuffer + instance->m_responseLength, evt->data, copyLength);
                instance->m_responseLength += copyLength;
                instance->m_responseBuffer[instance->m_responseLength] = '\0'; // Null-terminate
            }
        }
        break;
    default:
        break;
    }
    return ESP_OK;
}

// Fetch Bitcoin price - Performs an HTTP request and parses the response
bool BitcoinPriceFetcher::fetchBitcoinPrice()
{
    m_responseLength = 0; // Reset buffer

    esp_http_client_config_t config = {};
    config.url = getBTCAPI;
    config.event_handler = http_event_handler;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.user_data = this;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client.");
        return false;
    }

    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return false;
    }

    ESP_LOGI(TAG, "HTTP Status = %d, Content-Length = %lld", esp_http_client_get_status_code(client),
             esp_http_client_get_content_length(client));

    esp_http_client_cleanup(client);

    if (m_responseLength == 0) {
        ESP_LOGE(TAG, "Empty response received!");
        return false;
    }

    ESP_LOGI(TAG, "Received JSON: %s", m_responseBuffer);
    return parseBitcoinPrice(m_responseBuffer);
}

// Parse Bitcoin price from JSON response
bool BitcoinPriceFetcher::parseBitcoinPrice(const char *jsonData)
{
    cJSON *json = cJSON_Parse(jsonData);
    if (!json) {
        ESP_LOGE(TAG, "JSON parsing failed!");
        return false;
    }

    cJSON *usd = cJSON_GetObjectItem(json, "USD");
    if (!usd || !cJSON_IsNumber(usd)) {
        ESP_LOGE(TAG, "USD field missing or invalid.");
        cJSON_Delete(json);
        return false;
    }

    m_bitcoinPrice = static_cast<uint32_t>(usd->valuedouble);
    ESP_LOGI(TAG, "Bitcoin price in USD: %lu", m_bitcoinPrice);

    cJSON_Delete(json);
    return true;
}

// FreeRTOS task wrapper - Calls the main task function
void BitcoinPriceFetcher::taskWrapper(void *pvParameters)
{
    BitcoinPriceFetcher *instance = static_cast<BitcoinPriceFetcher *>(pvParameters);
    instance->task();
}

// FreeRTOS task function - Waits for an event and fetches data when enabled
void BitcoinPriceFetcher::task()
{
    ESP_LOGI(TAG, "Bitcoin Price Fetcher started...");

    // initial price fetching
    fetchBitcoinPrice();

    while (true) {
        pthread_mutex_lock(&m_mutex);
        pthread_cond_wait(&m_cond, &m_mutex); // Wait for enable signal
        pthread_mutex_unlock(&m_mutex);

        do {
            fetchBitcoinPrice();
            vTaskDelay(60000 / portTICK_PERIOD_MS); // Wait for 1 minute before fetching again
        } while (m_enabled);
    }
}