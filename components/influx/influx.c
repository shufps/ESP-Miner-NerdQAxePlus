#include <cJSON.h>
#include <esp_http_client.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "influx.h"

static const char * TAG = "InfluxDB";

static char auth_header[256];
static char big_buffer[16384];

bool influx_ping(Influx * influx)
{
    char url[256];
    snprintf(url, sizeof(url), "%s:%d/ping", influx->host, influx->port);
    ESP_LOGI(TAG, "URL: %s", url);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        esp_http_client_cleanup(client);

        if (status_code == 204) { // 204 No Content is the expected status code for /ping
            ESP_LOGI(TAG, "Successfully connected to InfluxDB at %s:%d", influx->host, influx->port);
            return true;
        } else {
            ESP_LOGE(TAG, "InfluxDB ping failed with status code: %d", status_code);
        }
    } else {
        ESP_LOGE(TAG, "InfluxDB ping request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return false;
}

bool bucket_exists(Influx *influx)
{
    char url[256];
    snprintf(url, sizeof(url), "%s:%d/api/v2/buckets?org=%s&name=%s", influx->host, influx->port, influx->org, influx->bucket);
    ESP_LOGI(TAG, "URL: %s", url);

    int content_length = 0;

    esp_http_client_config_t config = {
        .url = url,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // Set headers
    esp_http_client_set_header(client, "Authorization", auth_header);

    // Open connection
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return false;
    }

    // Fetch headers
    content_length = esp_http_client_fetch_headers(client);
    if (content_length < 0) {
        ESP_LOGE(TAG, "HTTP client fetch headers failed");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return false;
    }

    // Read response
    int data_read = esp_http_client_read_response(client, big_buffer, sizeof(big_buffer) - 1);
    if (data_read >= 0) {
        big_buffer[data_read] = 0;  // Null-terminate the response
        ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %d", esp_http_client_get_status_code(client), content_length);
        ESP_LOGD(TAG, "Response: %s", big_buffer);

        if (esp_http_client_get_status_code(client) == 200) {
            cJSON *json = cJSON_Parse(big_buffer);
            if (json == NULL) {
                ESP_LOGE(TAG, "Failed to parse JSON response");
                esp_http_client_close(client);
                esp_http_client_cleanup(client);
                return false;
            }

            cJSON *buckets = cJSON_GetObjectItem(json, "buckets");
            if (buckets != NULL && cJSON_IsArray(buckets) && cJSON_GetArraySize(buckets) > 0) {
                // If we get here, the bucket exists
                cJSON_Delete(json);
                esp_http_client_close(client);
                esp_http_client_cleanup(client);
                return true;
            } else {
                ESP_LOGW(TAG, "Bucket not found");
            }

            cJSON_Delete(json);
        } else if (esp_http_client_get_status_code(client) == 404) {
            ESP_LOGI(TAG, "Bucket not found");
        } else {
            ESP_LOGE(TAG, "HTTP GET failed with status code: %d", esp_http_client_get_status_code(client));
        }
    } else {
        ESP_LOGE(TAG, "Failed to read response");
    }

    // Close connection and cleanup
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return false;
}


void load_last_values(Influx *influx)
{
    char url[512];
    snprintf(url, sizeof(url), "%s:%d/api/v2/query?org=%s", influx->host, influx->port, influx->org);
    ESP_LOGI(TAG, "URL: %s", url);

    char query[1024];
    snprintf(query, sizeof(query),
             "from(bucket:\"%s\") |> range(start:-1y) |> filter(fn:(r) => r._measurement == \"%s\") |> last()", influx->bucket,
             influx->prefix);

    esp_http_client_config_t config = {
        .url = url,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // Set headers
    esp_http_client_set_header(client, "Authorization", auth_header);
    esp_http_client_set_header(client, "Content-Type", "application/vnd.flux");

    // Set POST field
    esp_http_client_set_post_field(client, query, strlen(query));

    // Open connection
    esp_err_t err = esp_http_client_open(client, strlen(query));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return;
    }

    // Fetch headers
    int content_length = esp_http_client_fetch_headers(client);
    if (content_length < 0) {
        ESP_LOGE(TAG, "HTTP client fetch headers failed");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return;
    }

    // Read response
    int data_read = esp_http_client_read_response(client, big_buffer, sizeof(big_buffer) - 1);
    if (data_read >= 0) {
        big_buffer[data_read] = 0;  // Null-terminate the response
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %d", esp_http_client_get_status_code(client), content_length);
        ESP_LOGD(TAG, "Response: %s", big_buffer);

        if (esp_http_client_get_status_code(client) == 200) {
            cJSON *json = cJSON_Parse(big_buffer);
            if (json == NULL) {
                ESP_LOGE(TAG, "Failed to parse JSON response");
                esp_http_client_close(client);
                esp_http_client_cleanup(client);
                return;
            }

            cJSON *table = cJSON_GetArrayItem(cJSON_GetObjectItem(json, "tables"), 0);
            if (table == NULL) {
                ESP_LOGW(TAG, "No data found for measurement %s", influx->prefix);
                cJSON_Delete(json);
                esp_http_client_close(client);
                esp_http_client_cleanup(client);
                return;
            }

            cJSON *record;
            cJSON_ArrayForEach(record, table) {
                const char *field = cJSON_GetObjectItem(record, "_field")->valuestring;
                double value = cJSON_GetObjectItem(record, "_value")->valuedouble;

                if (strcmp(field, "total_uptime") == 0)
                    influx->stats.total_uptime = (int)value;
                else if (strcmp(field, "total_best_difficulty") == 0)
                    influx->stats.total_best_difficulty = value;
                else if (strcmp(field, "total_blocks_found") == 0)
                    influx->stats.total_blocks_found = (int)value;
            }
            cJSON_Delete(json);
            ESP_LOGI(TAG, "Loaded last values from InfluxDB");
        } else {
            ESP_LOGE(TAG, "Failed to load last values, HTTP status: %d", esp_http_client_get_status_code(client));
        }
    } else {
        ESP_LOGE(TAG, "Failed to read response");
    }

    // Close connection and cleanup
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
}


void influx_write(Influx * influx)
{
    char url[256];
    struct timeval now;
    gettimeofday(&now, NULL);

    snprintf(big_buffer, sizeof(big_buffer),
             "%s temperature=%f,temperature2=%f,"
             "hashing_speed=%f,invalid_shares=%d,valid_shares=%d,uptime=%d,"
             "best_difficulty=%f,total_best_difficulty=%f,pool_errors=%d,"
             "accepted=%d,not_accepted=%d,total_uptime=%d,blocks_found=%d,"
             "total_blocks_found=%d,duplicate_hashes=%d %lld000000000",
             influx->prefix, influx->stats.temp, influx->stats.temp2, influx->stats.hashing_speed, influx->stats.invalid_shares,
             influx->stats.valid_shares, influx->stats.uptime, influx->stats.best_difficulty, influx->stats.total_best_difficulty,
             influx->stats.pool_errors, influx->stats.accepted, influx->stats.not_accepted, influx->stats.total_uptime,
             influx->stats.blocks_found, influx->stats.total_blocks_found, influx->stats.duplicate_hashes, (long long) now.tv_sec);

    snprintf(url, sizeof(url), "%s:%d/api/v2/write?bucket=%s&org=%s&precision=ns", influx->host, influx->port, influx->bucket,
             influx->org);

    ESP_LOGI(TAG, "URL: %s", url);
    ESP_LOGI(TAG, "POST: %s", big_buffer);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_header(client, "Authorization", auth_header);
    esp_http_client_set_header(client, "Content-Type", "text/plain");
    esp_http_client_set_post_field(client, big_buffer, strlen(big_buffer));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        int content_length = esp_http_client_get_content_length(client);

        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %d", status_code, content_length);

        if (status_code == 400) {
            int len = esp_http_client_read(client, big_buffer, sizeof(big_buffer) - 1);
            if (len > 0) {
                big_buffer[len] = 0; // Null-terminate the response
                ESP_LOGE(TAG, "HTTP POST Error 400 Response: %s", big_buffer);
            } else {
                ESP_LOGE(TAG, "HTTP POST Error 400: No response body");
            }
        }
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

Influx * influx_init(const char * host, int port, const char * token, const char * bucket, const char * org, const char * prefix)
{
    Influx * influxdb = (Influx *) malloc(sizeof(Influx));

    memset(influxdb, 0, sizeof(Influx));

    influxdb->port = port;
    influxdb->host = strdup(host);
    influxdb->token = strdup(token);
    influxdb->bucket = strdup(bucket);
    influxdb->prefix = strdup(prefix);
    influxdb->org = strdup(org);
    influxdb->lock = PTHREAD_MUTEX_INITIALIZER;

    // prepare auth header
    snprintf(auth_header, sizeof(auth_header), "Token %s", token);

    return influxdb;
}
