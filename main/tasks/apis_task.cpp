#include "apis_task.h"
#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
//#include "mbedtls/platform.h"
#include <cstring>

static const char *TAG = "APIsFetcher";
#define getBTCAPI "https://mempool.space/api/v1/prices"
#define getBLOCKHEIGHT "https://mempool.space/api/blocks/tip/height"
#define getGlobalHash "https://mempool.space/api/v1/mining/hashrate/3d"
#define getDifficulty "https://mempool.space/api/v1/difficulty-adjustment"
#define getFees "https://mempool.space/api/v1/fees/recommended"

#define MIN(a, b) ((a)<(b))?(a):(b)

// Constructor
APIsFetcher::APIsFetcher() {
    m_bitcoinPrice = 0;
    m_responseLength = 0;
    m_blockHeigh = 0;
    m_netHash = 0;
    m_netDifficulty = 0;
}

// Get latest Bitcoin price
uint32_t APIsFetcher::getPrice() {
    return m_bitcoinPrice;
}

// Get Block Height
uint32_t APIsFetcher::getBlockHeight() {
    return m_blockHeigh;
}

// Get latest Network hashrate
uint64_t APIsFetcher::getNetHash() {
    return m_netHash;
}
// Get latest Network difficulty
uint64_t APIsFetcher::getNetDifficulty() {
    return m_netDifficulty;
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

// Fetch Bitcoin price
bool APIsFetcher::fetchBitcoinPrice() {
    m_responseLength = 0; // Reset buffer

    esp_http_client_config_t config;
    memset(&config, 0, sizeof(esp_http_client_config_t));

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
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP Status = %d, Content-Length = %lld", esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));

        if (m_responseLength > 0) {
            ESP_LOGI(TAG, "Received JSON: %s", m_responseBuffer);

            // Parse JSON response
            cJSON *json = cJSON_Parse(m_responseBuffer);
            if (json) {
                cJSON *usd = cJSON_GetObjectItem(json, "USD");
                if (usd && cJSON_IsNumber(usd)) {
                    m_bitcoinPrice = (uint32_t) usd->valuedouble;
                    ESP_LOGI(TAG, "Bitcoin price in USD: %lu", m_bitcoinPrice);
                } else {
                    ESP_LOGE(TAG, "USD field missing or invalid.");
                }
                cJSON_Delete(json);
            } else {
                ESP_LOGE(TAG, "JSON parsing failed!");
            }
        } else {
            ESP_LOGE(TAG, "Empty response received!");
        }
    } else {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return false;
    }

    esp_http_client_cleanup(client);
    return true;
}

bool APIsFetcher::fetchBlockHeight(void){
    
    m_responseLength = 0; // Reset buffer

    esp_http_client_config_t config;
    memset(&config, 0, sizeof(esp_http_client_config_t));

    config.url = getBLOCKHEIGHT;
    config.event_handler = http_event_handler;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.user_data = this;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client.");
        return false;
    }

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP Status = %d, Content-Length = %lld", esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));

        if (m_responseLength > 0) {
            ESP_LOGI(TAG, "Received JSON: %s", m_responseBuffer);

            // Parse JSON response
            cJSON *json = cJSON_Parse(m_responseBuffer);
            if (json) {
                if (cJSON_IsNumber(json)) {
                    m_blockHeigh = (uint32_t) json->valuedouble;
                    ESP_LOGI(TAG, "Current block: %lu", m_blockHeigh);
                } else {
                    ESP_LOGE(TAG, "Height block field missing or invalid.");
                }
                cJSON_Delete(json);
            } else {
                ESP_LOGE(TAG, "JSON parsing failed!");
            }
        } else {
            ESP_LOGE(TAG, "Empty response received!");
        }
    } else {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return false;
    }

    esp_http_client_cleanup(client);
    return true;
}

bool APIsFetcher::fetchNetHash(void){
    
    m_responseLength = 0; // Reset buffer

    esp_http_client_config_t config;
    memset(&config, 0, sizeof(esp_http_client_config_t));

    config.url = getGlobalHash;
    config.event_handler = http_event_handler;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.user_data = this;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client.");
        return false;
    }

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int statusCode = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP Status = %d, Content-Length = %lld", statusCode,
                 esp_http_client_get_content_length(client));

            if (statusCode == 200 && m_responseLength > 0) {
            ESP_LOGI(TAG, "Received JSON: %s", m_responseBuffer);

            // Parse JSON response
            cJSON *json = cJSON_Parse(m_responseBuffer);
            if (json) {
                cJSON *netHash = cJSON_GetObjectItem(json, "currentHashrate");
                if (netHash && cJSON_IsNumber(netHash)) {
                    double rawHash = netHash->valuedouble; // Convertir a uint64_t
                    m_netHash = (uint64_t)(rawHash / 1e18); // Convertir a EH/s
                    ESP_LOGI(TAG, "Network hash: %llu EH/s", m_netHash);
                } else {
                    ESP_LOGE(TAG, "Network hash field missing or invalid.");
                }

                cJSON *netDiff = cJSON_GetObjectItem(json, "currentDifficulty");
                if (netDiff && cJSON_IsNumber(netDiff)) {
                    double rawDiff = netDiff->valuedouble; // Convertir a uint64_t
                    m_netDifficulty =  (uint64_t)(rawDiff / 1e12); // Convertir a Teras
                    ESP_LOGI(TAG, "Network difficulty: %llu T", m_netDifficulty);
                } else {
                    ESP_LOGE(TAG, "Network difficulty field missing or invalid.");
                }
                cJSON_Delete(json);
            } else {
                ESP_LOGE(TAG, "JSON parsing failed!");
            }
        } else {
            ESP_LOGE(TAG, "Empty response received!");
        }
    } else {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return false;
    }

    esp_http_client_cleanup(client);
    return true;
}

// FreeRTOS task wrapper (must be static)
void APIsFetcher::taskWrapper(void *pvParameters) {
    APIsFetcher *instance = static_cast<APIsFetcher *>(pvParameters);
    instance->task();
}

// FreeRTOS task function
void APIsFetcher::task() {
    ESP_LOGI(TAG, "Bitcoin Price Fetcher started...");
    while (true) {
        if (!fetchBitcoinPrice()) {
            ESP_LOGW(TAG, "Failed to fetch price");
        }
        if (!fetchBlockHeight()) {
            ESP_LOGW(TAG, "Failed to fetch Block Height");
        }
        if (!fetchNetHash()) {
            ESP_LOGW(TAG, "Failed to fetch Net Hash");
        }
        vTaskDelay(60000 / portTICK_PERIOD_MS);
    }
}

