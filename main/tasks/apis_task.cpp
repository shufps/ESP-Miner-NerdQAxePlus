#include "apis_task.h"
#include "ArduinoJson.h"
#include "psram_allocator.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/event_groups.h"
//#include "mbedtls/platform.h"
#include <cstring>

static const char *TAG = "APIsFetcher";
#define APIurl_BTCPRICE    "https://mempool.space/api/v1/prices"
#define APIurl_BLOCKHEIGHT "https://mempool.space/api/blocks/tip/height"
#define APIurl_GLOBALHASH  "https://mempool.space/api/v1/mining/hashrate/3d"
#define APIurl_GETFEES     "https://mempool.space/api/v1/fees/recommended"

#define MIN(a, b) ((a)<(b))?(a):(b)

#define FETCH_EVENT_BIT (1 << 0)

#define HALVING_BLOCKS 210000

// Constructor
APIsFetcher::APIsFetcher() {
    m_bitcoinPrice = 0;
    m_responseLength = 0;
    m_blockHeigh = 0;
    m_netHash = 0;
    m_netDifficulty = 0;
    m_hourFee = 0;
    m_halfHourFee = 0;
    m_fastestFee = 0;

    // Initialize mutex and condition variable
    pthread_mutex_init(&m_mutex, nullptr);
    pthread_cond_init(&m_cond, nullptr);
}

// Enable fetching - Wakes up the fetcher thread immediately
void APIsFetcher::enableFetching()
{
    pthread_mutex_lock(&m_mutex);
    m_enabled = true;
    pthread_cond_signal(&m_cond); // Wake up thread immediately
    pthread_mutex_unlock(&m_mutex);
}

// Disable fetching - Stops the fetching process
void APIsFetcher::disableFetching()
{
    m_enabled = false;
}

// Get latest Bitcoin price
uint32_t APIsFetcher::getPrice() {
    return m_bitcoinPrice;
}

// Get Block Height
uint32_t APIsFetcher::getBlockHeight() {
    return m_blockHeigh;
}

// Get Pending Halving blocks
uint32_t APIsFetcher::getBlocksToHalving() {
    if(!m_blockHeigh) return 0;
    return (((m_blockHeigh / HALVING_BLOCKS) + 1) * HALVING_BLOCKS) - m_blockHeigh;
}

// Get Pending Halving blocks
uint32_t APIsFetcher::getHalvingPercent() {
    return (HALVING_BLOCKS - getBlocksToHalving()) * 100 / HALVING_BLOCKS;;
}

// Get latest Network hashrate
uint64_t APIsFetcher::getNetHash() {
    return m_netHash;
}
// Get latest Network difficulty
uint64_t APIsFetcher::getNetDifficulty() {
    return m_netDifficulty;
}

// Get Lowest fee
uint32_t APIsFetcher::getLowestFee() {
    return m_hourFee;
}

// Get Mid fee
uint32_t APIsFetcher::getMidFee() {
    return m_halfHourFee;
}

// Get Fastest fee
uint32_t APIsFetcher::getFastestFee() {
    return m_fastestFee;
}

// HTTP event handler
esp_err_t APIsFetcher::http_event_handler(esp_http_client_event_t *evt) {
    APIsFetcher *instance = static_cast<APIsFetcher *>(evt->user_data);

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

// Fetch Data - Performs an HTTP request and parses the response
bool APIsFetcher::fetchData(const char* apiUrl, ApiType type)
{
    m_responseLength = 0; // Reset buffer

    esp_http_client_config_t config = {};
    config.url = apiUrl;
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

    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);

    DeserializationError error = deserializeJson(doc, m_responseBuffer);
    if (error) {
        ESP_LOGE(TAG, "JSON parsing failed!");
        return false;
    }

    switch (type) {
        case APItype_PRICE:
            return parseBitcoinPrice(doc);
        case APItype_BLOCK_HEIGHT:
            return parseBlockHeight(doc);
        case APItype_HASHRATE:
            return parseHashrate(doc);
        case APItype_FEES:
            return parseFees(doc);
        default:
            ESP_LOGE(TAG, "Unknown API type.");
            return false;
    }
}

// Parse Bitcoin price
bool APIsFetcher::parseBitcoinPrice(JsonDocument &doc) {
    m_bitcoinPrice = doc["USD"].as<uint32_t>();
    ESP_LOGI(TAG, "Bitcoin price in USD: %lu", m_bitcoinPrice);
    return true;
}

// Parse Bloack Height
bool APIsFetcher::parseBlockHeight(JsonDocument &doc) {
    m_blockHeigh = doc.as<uint32_t>();
    ESP_LOGI(TAG, "Current block: %lu", m_blockHeigh);
    return true;
}

bool APIsFetcher::parseHashrate(JsonDocument &doc) {
    double rawHash = doc["currentHashrate"].as<double>();  // Read as floating point
    double rawDiff = doc["currentDifficulty"].as<double>();  // Read as floating point

    m_netHash = static_cast<uint64_t>(rawHash / 1e18); // Convert to EH/s
    m_netDifficulty = static_cast<uint64_t>(rawDiff / 1e12); // Convert to T

    ESP_LOGI(TAG, "Network hash: %llu EH/s", m_netHash);
    ESP_LOGI(TAG, "Network difficulty: %llu T", m_netDifficulty);

    return true;
}


bool APIsFetcher::parseFees(JsonDocument &doc) {
    m_hourFee = doc["hourFee"].as<uint32_t>();
    m_halfHourFee = doc["halfHourFee"].as<uint32_t>();
    m_fastestFee = doc["fastestFee"].as<uint32_t>();
    ESP_LOGI(TAG, "Network fees: %lu, %lu, %lu", m_hourFee, m_halfHourFee, m_fastestFee);
    return true;
}


// FreeRTOS task wrapper (must be static)
void APIsFetcher::taskWrapper(void *pvParameters) {
    APIsFetcher *instance = static_cast<APIsFetcher *>(pvParameters);
    instance->task();
}

// FreeRTOS task function
void APIsFetcher::task() {
    ESP_LOGI(TAG, "APIs Fetcher started...");

    // initial price fetching
    fetchData(APIurl_BTCPRICE, APItype_PRICE);
    fetchData(APIurl_BLOCKHEIGHT, APItype_BLOCK_HEIGHT);
    fetchData(APIurl_GLOBALHASH, APItype_HASHRATE);
    fetchData(APIurl_GETFEES, APItype_FEES);

    while (true) {
        pthread_mutex_lock(&m_mutex);
        pthread_cond_wait(&m_cond, &m_mutex); // Wait for enable signal
        pthread_mutex_unlock(&m_mutex);

        do{
            fetchData(APIurl_BTCPRICE, APItype_PRICE);
            fetchData(APIurl_BLOCKHEIGHT, APItype_BLOCK_HEIGHT);
            fetchData(APIurl_GLOBALHASH, APItype_HASHRATE);
            fetchData(APIurl_GETFEES, APItype_FEES);

            vTaskDelay(pdMS_TO_TICKS(60000));
        }while (m_enabled);
    }
}

