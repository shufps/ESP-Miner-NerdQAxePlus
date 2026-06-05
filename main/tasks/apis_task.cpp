#include "apis_task.h"
#include "ArduinoJson.h"
#include "psram_allocator.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/event_groups.h"
#include "mbedtls/error.h"
//#include "mbedtls/platform.h"
#include <cstring>
#include "nvs_config.h"
#include "macros.h"

static const char *TAG = "APIsFetcher";

// API paths (appended to the configurable base URL)
#define API_PATH_BTCPRICE    "/api/v1/prices"
#define API_PATH_BLOCKHEIGHT "/api/blocks/tip/height"
#define API_PATH_GLOBALHASH  "/api/v1/mining/hashrate/3d"
#define API_PATH_GETFEES     "/api/v1/fees/recommended"

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
        case HTTP_EVENT_ERROR:
        case HTTP_EVENT_DISCONNECTED: {
            int errnum = esp_http_client_get_errno(evt->client);
            if (errnum) {
                ESP_LOGE("APIsFetcher", "HTTP connection error/disconnected, errno=%d, transport=%s",
                    errnum,
                    esp_http_client_get_transport_type(evt->client)==HTTP_TRANSPORT_OVER_SSL?"SSL":"TCP");
            }
            break;
        }

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

        ESP_LOGE(TAG, "HTTP request error details: errno=%d, transport=%s",
                 esp_http_client_get_errno(client),
                 esp_http_client_get_transport_type(client)==HTTP_TRANSPORT_OVER_SSL?"SSL":"TCP");

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

// Build full URL from base + path. Caller must free() the result.
static char* buildUrl(const char* base, const char* path)
{
    // Strip trailing slash from base if present
    size_t baseLen = strlen(base);
    while (baseLen > 0 && base[baseLen - 1] == '/') baseLen--;

    size_t len = baseLen + strlen(path) + 1;
    char* url = (char*) MALLOC(len);
    if (url) {
        snprintf(url, len, "%.*s%s", (int) baseLen, base, path);
    }
    return url;
}

void APIsFetcher::fetchAll()
{
    char* baseUrl = Config::isMempoolCustom() ? Config::getMempoolUrl() : strdup(CONFIG_MEMPOOL_URL);
    if (!baseUrl || baseUrl[0] == '\0') {
        ESP_LOGI(TAG, "Mempool URL not configured, skipping fetch");
        m_bitcoinPrice = 0;
        m_blockHeigh = 0;
        m_netHash = 0;
        m_netDifficulty = 0;
        m_hourFee = 0;
        m_halfHourFee = 0;
        m_fastestFee = 0;
        free(baseUrl);
        return;
    }

    const char* paths[] = { API_PATH_BTCPRICE, API_PATH_BLOCKHEIGHT, API_PATH_GLOBALHASH, API_PATH_GETFEES };
    const ApiType types[] = { APItype_PRICE, APItype_BLOCK_HEIGHT, APItype_HASHRATE, APItype_FEES };

    for (int i = 0; i < 4; i++) {
        char* url = buildUrl(baseUrl, paths[i]);
        if (url) {
            fetchData(url, types[i]);
            free(url);
        }
    }

    free(baseUrl);
}

// FreeRTOS task function
void APIsFetcher::task() {
    ESP_LOGI(TAG, "APIs Fetcher started...");

    // initial fetching
    fetchAll();

    while (true) {
        pthread_mutex_lock(&m_mutex);
        pthread_cond_wait(&m_cond, &m_mutex); // Wait for enable signal
        pthread_mutex_unlock(&m_mutex);

        do{
            fetchAll();
#if 0
            UBaseType_t watermark = uxTaskGetStackHighWaterMark(NULL);
            ESP_LOGI(TAG, "Stack high watermark: %u bytes", watermark);
#endif
            vTaskDelay(pdMS_TO_TICKS(60000));
        }while (m_enabled);
    }
}

