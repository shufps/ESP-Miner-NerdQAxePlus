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



typedef struct
{
    char * host;
    int port;
    char * token;
    char * org;
    char * bucket;
    char * stats_name;
    SemaphoreHandle_t lock;
} Influx;

static Stats stats;


static Influx influxdb;

void influx_connect(Influx * influx)
{
    ESP_LOGI(TAG, "Connecting to InfluxDB at %s:%d", influx->host, influx->port);
}

bool bucket_exists(Influx * influx)
{
    char url[256];
    snprintf(url, sizeof(url), "http://%s:%d/api/v2/buckets?org=%s", influx->host, influx->port, influx->org);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_header(client, "Authorization", "Token your_token");

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        if (status_code == 200) {
            char buffer[1024];
            int len = esp_http_client_read(client, buffer, sizeof(buffer) - 1);
            buffer[len] = 0;

            cJSON * json = cJSON_Parse(buffer);
            cJSON * buckets = cJSON_GetObjectItem(json, "buckets");
            cJSON * bucket;
            cJSON_ArrayForEach(bucket, buckets)
            {
                const char * name = cJSON_GetObjectItem(bucket, "name")->valuestring;
                if (strcmp(name, influx->bucket) == 0) {
                    cJSON_Delete(json);
                    esp_http_client_cleanup(client);
                    return true;
                }
            }
            cJSON_Delete(json);
        }
    } else {
        ESP_LOGE(TAG, "Bucket check failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return false;
}

void load_last_values(Influx * influx)
{
    if (!bucket_exists(influx)) {
        ESP_LOGI(TAG, "Bucket %s does not exist. Nothing imported.", influx->bucket);
        return;
    }

    char url[512];
    snprintf(url, sizeof(url), "http://%s:%d/api/v2/query?org=%s", influx->host, influx->port, influx->org);

    char query[1024];
    snprintf(query, sizeof(query),
             "from(bucket:\"%s\") |> range(start:-1y) |> filter(fn:(r) => r._measurement == \"%s\") |> last()", influx->bucket,
             influx->stats_name);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_header(client, "Authorization", "Token your_token");
    esp_http_client_set_header(client, "Content-Type", "application/vnd.flux");

    esp_http_client_set_post_field(client, query, strlen(query));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        if (status_code == 200) {
            char buffer[2048];
            int len = esp_http_client_read(client, buffer, sizeof(buffer) - 1);
            buffer[len] = 0;

            cJSON * json = cJSON_Parse(buffer);
            cJSON * table = cJSON_GetArrayItem(cJSON_GetObjectItem(json, "tables"), 0);
            cJSON * record;
            cJSON_ArrayForEach(record, table)
            {
                const char * field = cJSON_GetObjectItem(record, "_field")->valuestring;
                double value = cJSON_GetObjectItem(record, "_value")->valuedouble;

                if (strcmp(field, "total_uptime") == 0)
                    stats.total_uptime = (int) value;
                else if (strcmp(field, "total_best_difficulty") == 0)
                    stats.total_best_difficulty = value;
                else if (strcmp(field, "total_blocks_found") == 0)
                    stats.total_blocks_found = (int) value;
            }
            cJSON_Delete(json);
            ESP_LOGI(TAG, "Loaded last values from InfluxDB");
        } else {
            ESP_LOGE(TAG, "Failed to load last values, HTTP status: %d", status_code);
        }
    } else {
        ESP_LOGE(TAG, "Failed to load last values: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

void influx_write(Influx * influx)
{
    char post_data[1024];
    struct timeval now;
    gettimeofday(&now, NULL);

    snprintf(post_data, sizeof(post_data),
             "measurement_name temperature=%f,temperature2=%f,"
             "hashing_speed=%f,invalid_shares=%d,valid_shares=%d,uptime=%d,"
             "best_difficulty=%f,total_best_difficulty=%f,pool_errors=%d,"
             "accepted=%d,not_accepted=%d,total_uptime=%d,blocks_found=%d,"
             "total_blocks_found=%d,duplicate_hashes=%d %lld000000",
             stats.temp, stats.temp2, stats.hashing_speed, stats.invalid_shares,
             stats.valid_shares, stats.uptime, stats.best_difficulty,
             stats.total_best_difficulty, stats.pool_errors, stats.accepted, stats.not_accepted,
             stats.total_uptime, stats.blocks_found, stats.total_blocks_found,
             stats.duplicate_hashes, (long long) now.tv_sec);

    esp_http_client_config_t config = {
        .url = "http://your_influxdb_server/write?db=your_database",
        .method = HTTP_METHOD_POST,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_header(client, "Authorization", "Token your_token");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %d", (int) esp_http_client_get_status_code(client),
                 (int) esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

void influx_task(void * pvParameter)
{
    Influx * influx = (Influx *) pvParameter;

    while (1) {
        xSemaphoreTake(influx->lock, portMAX_DELAY);
        influx_write(influx);
        xSemaphoreGive(influx->lock);
        vTaskDelay(pdMS_TO_TICKS(15000)); // Delay for 15 seconds
    }
}

void influx_init(const char* host, int port, const char* token, const char* bucket, const char* stats_name) {
    memset(&influxdb, 0, sizeof(Influx));
    influxdb.port = port;
    influxdb.host = strdup(host);
    influxdb.token = strdup(token);
    influxdb.bucket = strdup(bucket);
    influxdb.stats_name = strdup(stats_name);

    memset(&stats, 0, sizeof(Stats));

    influxdb.lock = xSemaphoreCreateMutex();
    influx_connect(&influxdb);
    load_last_values(&influxdb);
    xTaskCreate(&influx_task, "influx_task", 4096, &influxdb, 5, NULL);
}

