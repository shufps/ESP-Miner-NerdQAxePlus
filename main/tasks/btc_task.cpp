#include "btc_task.h"
#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
//#include "mbedtls/platform.h"
#include <cstring>

static const char *TAG = "BitcoinFetcher";
#define getBTCAPI "https://mempool.space/api/v1/prices"

#define MIN(a, b) ((a)<(b))?(a):(b)

// Constructor
BitcoinPriceFetcher::BitcoinPriceFetcher() {
    m_bitcoinPrice = 0;
    m_responseLength = 0;

}

// Get latest Bitcoin price
uint32_t BitcoinPriceFetcher::getPrice() {
    return m_bitcoinPrice;
}

// HTTP event handler
esp_err_t BitcoinPriceFetcher::http_event_handler(esp_http_client_event_t *evt) {
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

// Fetch Bitcoin price
bool BitcoinPriceFetcher::fetchBitcoinPrice() {
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

// FreeRTOS task wrapper (must be static)
void BitcoinPriceFetcher::taskWrapper(void *pvParameters) {
    BitcoinPriceFetcher *instance = static_cast<BitcoinPriceFetcher *>(pvParameters);
    instance->task();
}

// FreeRTOS task function
void BitcoinPriceFetcher::task() {
    ESP_LOGI(TAG, "Bitcoin Price Fetcher started...");
    while (true) {
        if (!fetchBitcoinPrice()) {
            ESP_LOGW(TAG, "Failed to fetch price. Retrying in 10s...");
            vTaskDelay(10000 / portTICK_PERIOD_MS);
            continue;
        }
        vTaskDelay(60000 / portTICK_PERIOD_MS);
    }
}

