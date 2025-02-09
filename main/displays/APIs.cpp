#include "APIs.h"
#include "cJSON.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "nvs_flash.h"
#include <stdlib.h>
#include <string.h>

#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

// Root CA certificate for mempool.space
static const char *root_ca_certificate = R"(-----BEGIN CERTIFICATE-----
MIIHxTCCBq2gAwIBAgIRAOSAJoOKT4jBOct9qaYtamkwDQYJKoZIhvcNAQELBQAw
gZUxCzAJBgNVBAYTAkdCMRswGQYDVQQIExJHcmVhdGVyIE1hbmNoZXN0ZXIxEDAO
BgNVBAcTB1NhbGZvcmQxGDAWBgNVBAoTD1NlY3RpZ28gTGltaXRlZDE9MDsGA1UE
AxM0U2VjdGlnbyBSU0EgT3JnYW5pemF0aW9uIFZhbGlkYXRpb24gU2VjdXJlIFNl
cnZlciBDQTAeFw0yNTAxMjAwMDAwMDBaFw0yNTA5MDQyMzU5NTlaMFUxCzAJBgNV
BAYTAkpQMQ4wDAYDVQQIEwVUb2t5bzEeMBwGA1UEChMVTWVtcG9vbCBTcGFjZSBD
byBMdGQuMRYwFAYDVQQDEw1tZW1wb29sLnNwYWNlMIIBIjANBgkqhkiG9w0BAQEF
AAOCAQ8AMIIBCgKCAQEAqpoj0VoKOfHduUdG4kIwJLFzKuXIh+8+2UKNsNgGwax8
aZ5SK8u+G/UuMEWOVCXmJnemcAO+jXUoqA78UyXNvWy6ZWPhbZn3OBE9M2G9pBYv
Os+xjlm+hM6K/6ns/bjDPXMXdNtQb3LNP9jqy+koKeurvZHg2b/TcrxT6V37kbXV
dq+Bxee1SamZ9dOTzeJPNJDVoA22nvGsPgblDb0GChHu9cnwewYes0ofaQknzCOq
SCXUM6GnWtrmNXUDKCXiKvKijSqgDyhrB+THHplE+WSClP7Y5N5319Kc/7EGuAje
7Sx+lHlJXLXovdhhXel7Z+0Wyi4Q1G3OEqtNgnPYlwIDAQABo4IETTCCBEkwHwYD
VR0jBBgwFoAUF9nWJSdn+THCSUPZMDZEjGypT+swHQYDVR0OBBYEFF5MaHXSeqyq
G7G5g3bbgitcsF7iMA4GA1UdDwEB/wQEAwIFoDAMBgNVHRMBAf8EAjAAMB0GA1Ud
JQQWMBQGCCsGAQUFBwMBBggrBgEFBQcDAjBKBgNVHSAEQzBBMDUGDCsGAQQBsjEB
AgEDBDAlMCMGCCsGAQUFBwIBFhdodHRwczovL3NlY3RpZ28uY29tL0NQUzAIBgZn
gQwBAgIwWgYDVR0fBFMwUTBPoE2gS4ZJaHR0cDovL2NybC5zZWN0aWdvLmNvbS9T
ZWN0aWdvUlNBT3JnYW5pemF0aW9uVmFsaWRhdGlvblNlY3VyZVNlcnZlckNBLmNy
bDCBigYIKwYBBQUHAQEEfjB8MFUGCCsGAQUFBzAChklodHRwOi8vY3J0LnNlY3Rp
Z28uY29tL1NlY3RpZ29SU0FPcmdhbml6YXRpb25WYWxpZGF0aW9uU2VjdXJlU2Vy
dmVyQ0EuY3J0MCMGCCsGAQUFBzABhhdodHRwOi8vb2NzcC5zZWN0aWdvLmNvbTCC
AX8GCisGAQQB1nkCBAIEggFvBIIBawFpAHYA3dzKNJXX4RYF55Uy+sef+D0cUN/b
ADoUEnYKLKy7yCoAAAGUg1LFrAAABAMARzBFAiBka75perfBeQK1036ohNwYCZt3
FEhrNrlihkFsN6li5QIhAM1zsOLHsuOUyfOKrrzjymsLEXM9Y2fcHaqYJ3zG9JS+
AHYAzPsPaoVxCWX+lZtTzumyfCLphVwNl422qX5UwP5MDbAAAAGUg1LFeAAABAMA
RzBFAiEAoQNw4JgiF7lf8PERFFhSazPjkbaDD5IP1Eirv4Kgc7QCIHrWskIBaQPp
MyLykVLnQX9HImgmLoj12DS6sXXhuSssAHcAEvFONL1TckyEBhnDjz96E/jntWKH
iJxtMAWE6+WGJjoAAAGUg1LFRQAABAMASDBGAiEA5u3m1SBv69wy68vQAmf6nA9H
aaZKci1Mst+ZKgH296oCIQCFTx6EEVEBnLaxbmB5Bxj6Ia1cP/MdD5UmCsgoo6zC
FDCCARAGA1UdEQSCAQcwggEDgg1tZW1wb29sLnNwYWNlghMqLmZtdC5tZW1wb29s
LnNwYWNlghMqLmZyYS5tZW1wb29sLnNwYWNlghMqLmhubC5tZW1wb29sLnNwYWNl
ghAqLm1lbWVwb29sLnNwYWNlgg8qLm1lbXBvb2wuc3BhY2WCEyouc2cxLm1lbXBv
b2wuc3BhY2WCEyouc3YxLm1lbXBvb2wuc3BhY2WCEyoudGs3Lm1lbXBvb2wuc3Bh
Y2WCEyoudmExLm1lbXBvb2wuc3BhY2WCDmJpdGNvaW4uZ29iLnN2gg5saXF1aWQu
bmV0d29ya4IObWVtZXBvb2wuc3BhY2WCDG1lbXBvby5zcGFjZTANBgkqhkiG9w0B
AQsFAAOCAQEAJsAbhwiKEV5q4hkOHLUPxQM9STe1IMLdDH3h7IDHcZC76Xojpqcj
a7kfpXSGS6gl8iyj9ef4AVaqt5Pn09OrBbBgg97kRam5d3QiLgH3aaNZcxhPXaFT
3yYBOp1nFY19k5XYMmBCGcFsUkjO8iS606XINDXImy1W4Jo4I/KGER7uyxAht2G9
a5LCaMvKE4c+NkTDgaxO8DuLKUKHsBWh3NjHFSvDNzuCAO+QMnFiJVzuoldGNjTL
BiZyQUIWtqBIBqt9ciWJ8KtiktYFDLbFyJx9c8+lE/yjU/zHEPqBIHyXYd/2cntq
AyN/otgqN4TgcbsxWpIIrzRnQ80ON3GiBA==
-----END CERTIFICATE-----
)";

static char tag[] = "APIsHelper";
static char *response_buffer = NULL; // Buffer para acumular la respuesta completa
static int response_length = 0;      // Longitud actual del buffer
static unsigned long mBTCUpdate = 0;
static unsigned int bitcoin_price = 0; // Establece esta variable globalmente

// Handler de eventos para la respuesta HTTP
esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
    case HTTP_EVENT_ON_DATA: {
        if (!esp_http_client_is_chunked_response(evt->client)) {
            // Aumentar el buffer con nuevos datos
            char *new_buffer = (char*) realloc(response_buffer, response_length + evt->data_len + 1);
            if (new_buffer == NULL) {
                ESP_LOGE(tag, "Failed to allocate memory for response buffer");
                return ESP_ERR_NO_MEM;
            }
            response_buffer = new_buffer;
            memcpy(response_buffer + response_length, (char *) evt->data, evt->data_len);
            response_length += evt->data_len;
            response_buffer[response_length] = '\0'; // Asegurarse de que el buffer es una cadena válida
        }
        break;
    }
    case HTTP_EVENT_ON_FINISH: {
        // Intentar parsear el JSON completo al final de la transmisión
        // ESP_LOGI(tag, "Final JSON received: %s", response_buffer);
        cJSON *json = cJSON_Parse(response_buffer);
        if (json != NULL) {
            cJSON *usd = cJSON_GetObjectItem(json, "USD");
            if (usd != NULL && cJSON_IsNumber(usd)) {
                bitcoin_price = (int) usd->valuedouble;
                ESP_LOGI(tag, "Bitcoin price in USD: %d", bitcoin_price);
            } else {
                ESP_LOGE(tag, "Failed to get USD price from JSON");
            }
            cJSON_Delete(json);
        } else {
            ESP_LOGE(tag, "Failed to parse JSON");
        }
        // Liberar el buffer después de procesar
        free(response_buffer);
        response_buffer = NULL;
        response_length = 0;
        break;
    }
    default:
        break;
    }
    return ESP_OK;
}

unsigned int getBTCprice(void)
{
    if ((mBTCUpdate == 0) || (esp_timer_get_time() / 1000 - mBTCUpdate > UPDATE_BTC_min * 60)) {
        esp_http_client_config_t config = {
            .url = getBTCAPI,
            .cert_pem = root_ca_certificate,
            .event_handler = http_event_handler,
            .skip_cert_common_name_check = true
        };

        esp_http_client_handle_t client = esp_http_client_init(&config);
        esp_err_t err = esp_http_client_perform(client);

        if (err == ESP_OK) {
            ESP_LOGI("HTTP", "HTTP Status = %d, content_length = %lld", esp_http_client_get_status_code(client),
                     esp_http_client_get_content_length(client));
            mBTCUpdate = esp_timer_get_time() / 1000;
        } else {
            ESP_LOGE("HTTP", "HTTP GET request failed: %s", esp_err_to_name(err));
        }

        esp_http_client_cleanup(client);
    }

    return bitcoin_price;
}
