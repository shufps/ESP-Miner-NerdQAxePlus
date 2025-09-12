#include "ArduinoJson.h"
#include "psram_allocator.h"
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
#include "macros.h"

#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

static const char *TAG = "InfluxDB";

#define m_big_buffer_SIZE 32768

Influx::Influx() {
    // nop
}

bool Influx::ping()
{
    char url[256];
    snprintf(url, sizeof(url), "%s:%d/ping", m_host, m_port);
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
            ESP_LOGI(TAG, "Successfully connected to InfluxDB at %s:%d", m_host, m_port);
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

bool Influx::get_org_id(char *out_org_id, size_t max_len) {
    char url[256];
    snprintf(url, sizeof(url), "%s:%d/api/v2/orgs?org=%s", m_host, m_port, m_org);
    ESP_LOGI(TAG, "Looking up orgID via: %s", url);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Authorization", m_auth_header);
    esp_http_client_set_header(client, "Accept", "application/json");

    int len = 0;
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return false;
    }

    len = esp_http_client_fetch_headers(client);
    if (len < 0) {
        ESP_LOGE(TAG, "Failed to fetch headers");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return false;
    }

    len = esp_http_client_read_response(client, m_big_buffer, m_big_buffer_SIZE - 1);
    if (len <= 0) {
        ESP_LOGE(TAG, "Failed to read response");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return false;
    }
    m_big_buffer[len] = '\0';

    ESP_LOGI(TAG, "Org lookup response: %s", m_big_buffer);

    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);
    DeserializationError json_err = deserializeJson(doc, m_big_buffer);
    if (json_err) {
        ESP_LOGE(TAG, "Failed to parse JSON: %s", json_err.c_str());
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return false;
    }

    const char *id = doc["orgs"][0]["id"];
    if (!id) {
        ESP_LOGE(TAG, "Failed to extract org ID from JSON");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return false;
    }

    strncpy(out_org_id, id, max_len - 1);
    out_org_id[max_len - 1] = '\0';
    ESP_LOGI(TAG, "Resolved orgID: %s", out_org_id);

    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return true;
}



bool Influx::create_bucket() {
    char url[256];
    snprintf(url, sizeof(url), "%s:%d/api/v2/buckets", m_host, m_port);

    ESP_LOGI(TAG, "Creating bucket at URL: %s", url);

    // Prepare JSON body
    char body[512];
    char org_id[64];
    if (!get_org_id(org_id, sizeof(org_id))) {
        ESP_LOGE(TAG, "Could not resolve org ID");
        return false;
    }

    snprintf(body, sizeof(body),
            "{\"orgID\": \"%s\", \"name\": \"%s\", \"description\": \"Auto-created\", \"retentionRules\": [{\"type\": \"expire\", \"everySeconds\": 2592000}]}",
            org_id, m_bucket);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_header(client, "Authorization", m_auth_header);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, body, strlen(body));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        if (status_code == 201) {
            ESP_LOGI(TAG, "Bucket created successfully.");
            esp_http_client_cleanup(client);
            return true;
        } else {
            ESP_LOGE(TAG, "Failed to create bucket, status: %d", status_code);
        }
    } else {
        ESP_LOGE(TAG, "HTTP POST to create bucket failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return false;
}

bool Influx::bucket_exists()
{
    char url[256];
    snprintf(url, sizeof(url), "%s:%d/api/v2/buckets?org=%s&name=%s", m_host, m_port, m_org, m_bucket);
    ESP_LOGI(TAG, "URL: %s", url);

    int content_length = 0;

    esp_http_client_config_t config = {
        .url = url,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // Set headers
    esp_http_client_set_header(client, "Authorization", m_auth_header);

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
    int data_read = esp_http_client_read_response(client, m_big_buffer, m_big_buffer_SIZE - 1);
    if (data_read >= 0) {
        m_big_buffer[data_read] = 0; // Null-terminate the response
        ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %d", esp_http_client_get_status_code(client), content_length);
        ESP_LOGD(TAG, "Response: %s", m_big_buffer);

        if (esp_http_client_get_status_code(client) == 200) {
            PSRAMAllocator allocator;
            JsonDocument doc(&allocator);

            // Deserialize JSON
            DeserializationError error = deserializeJson(doc, m_big_buffer);
            if (error) {
                ESP_LOGE(TAG, "Failed to parse JSON response: %s", error.c_str());
                esp_http_client_close(client);
                esp_http_client_cleanup(client);
                return false;
            }

            //ESP_LOGI(TAG, "allocs: %d, deallocs: %d, reallocs: %d", allocs, deallocs, reallocs);

            // Extract "buckets" array
            JsonArray buckets = doc["buckets"].as<JsonArray>();
            if (!buckets.isNull() && buckets.size() > 0) {
                // If we get here, the bucket exists
                esp_http_client_close(client);
                esp_http_client_cleanup(client);
                return true;
            } else {
                ESP_LOGW(TAG, "Bucket not found");
            }
        } else if (esp_http_client_get_status_code(client) == 404) {
            ESP_LOGI(TAG, "Bucket list request error");
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



bool Influx::load_last_values()
{
    char url[256];
    snprintf(url, sizeof(url), "%s:%d/api/v2/query?org=%s", m_host, m_port, m_org);
    ESP_LOGI(TAG, "URL: %s", url);

    // Construct the JSON object with the Flux query in one step
    char query_json[256];
    snprintf(
        query_json, sizeof(query_json),
        "{\"query\":\"from(bucket:\\\"%s\\\") |> range(start:-1y) |> filter(fn:(r) => r._measurement == \\\"%s\\\") |> last()\"}",
        m_bucket, m_prefix);

    ESP_LOGI(TAG, "Query JSON: %s", query_json);

    esp_http_client_config_t config = {.url = url, .method = HTTP_METHOD_POST};
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // Set headers
    esp_http_client_set_header(client, "Authorization", m_auth_header);
    esp_http_client_set_header(client, "Content-Type", "application/json"); // Set Content-Type to JSON
    esp_http_client_set_header(client, "Accept", "text/csv");

    // Open connection
    esp_err_t err = esp_http_client_open(client, strlen(query_json));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return false;
    }

    int wlen = esp_http_client_write(client, query_json, strlen(query_json));
    if (wlen < 0) {
        ESP_LOGE(TAG, "Write failed");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return false;
    }

    // Fetch headers
    int content_length = esp_http_client_fetch_headers(client);
    if (content_length < 0) {
        ESP_LOGE(TAG, "HTTP client fetch headers failed");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return false;
    }

    ESP_LOGI(TAG, "Expected content length: %d", content_length);

    // Read response
    int data_read = esp_http_client_read_response(client, m_big_buffer, m_big_buffer_SIZE - 1);
    if (data_read > 0) {
        m_big_buffer[data_read] = 0; // Null-terminate the response
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %d", esp_http_client_get_status_code(client), content_length);
        // ESP_LOGI(TAG, "Response: %s", m_big_buffer);

        if (esp_http_client_get_status_code(client) == 200) {
            // Parse CSV response with CRLF line endings
            char *saveptr1, *saveptr2;

            // the first strtok_r skips the CSV header
            char *line = strtok_r(m_big_buffer, "\r\n", &saveptr1); // Handle CRLF line endings

            // now parse the actual data
            while ((line = strtok_r(NULL, "\r\n", &saveptr1)) != NULL) {
                // ESP_LOGI(TAG, "line: '%s'", line);

                char *token;
                // Fields from CSV: result,table,_start,_stop,_time,_value,_field,_measurement
                strtok_r(line, ",", &saveptr2); // Skip result column
                strtok_r(NULL, ",", &saveptr2); // Skip table column
                strtok_r(NULL, ",", &saveptr2); // Skip _start column
                strtok_r(NULL, ",", &saveptr2); // Skip _stop column
                strtok_r(NULL, ",", &saveptr2); // Skip _time column

                // Get _value
                token = strtok_r(NULL, ",", &saveptr2);
                if (token == NULL) {
                    ESP_LOGE(TAG, "Failed to parse _value");
                    esp_http_client_close(client);
                    esp_http_client_cleanup(client);
                    return false;
                }
                // ESP_LOGI(TAG, "Parsing _value: %s", token);
                double value = atof(token);

                // Get _field
                token = strtok_r(NULL, ",", &saveptr2);
                if (token == NULL) {
                    ESP_LOGE(TAG, "Failed to parse _field");
                    esp_http_client_close(client);
                    esp_http_client_cleanup(client);
                    return false;
                }
                char *field = token;

                // Assign the parsed values to the appropriate fields
                if (strcmp(field, "total_uptime") == 0)
                    m_stats.total_uptime = (int) value;
                else if (strcmp(field, "total_best_difficulty") == 0)
                    m_stats.total_best_difficulty = value;
                else if (strcmp(field, "total_blocks_found") == 0)
                    m_stats.total_blocks_found = (int) value;
            }
            ESP_LOGI(TAG, "Loaded last values from InfluxDB");
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            return true;
        } else {
            ESP_LOGE(TAG, "Failed to load last values, HTTP status: %d", esp_http_client_get_status_code(client));
        }
    } else if (data_read == 0) {
        ESP_LOGW(TAG, "Received empty response");
    } else {
        ESP_LOGE(TAG, "Failed to read response");
    }

    // Close connection and cleanup
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return false;
}

void Influx::write()
{
    char url[256];

    snprintf(m_big_buffer, m_big_buffer_SIZE,
             "%s temperature=%f,temperature2=%f,"
             "hashing_speed=%f,invalid_shares=%d,valid_shares=%d,uptime=%d,"
             "best_difficulty=%f,total_best_difficulty=%f,pool_errors=%d,"
             "accepted=%d,not_accepted=%d,total_uptime=%d,blocks_found=%d,"
             "pwr_vin=%f,pwr_iin=%f,pwr_pin=%f,pwr_vout=%f,pwr_iout=%f,pwr_pout=%f,"
             "total_blocks_found=%d,duplicate_hashes=%d,last_ping_rtt=%.2f",
             m_prefix, m_stats.temp, m_stats.temp2, m_stats.hashing_speed, m_stats.invalid_shares,
             m_stats.valid_shares, m_stats.uptime, m_stats.best_difficulty, m_stats.total_best_difficulty,
             m_stats.pool_errors, m_stats.accepted, m_stats.not_accepted, m_stats.total_uptime,
             m_stats.blocks_found, m_stats.pwr_vin, m_stats.pwr_iin, m_stats.pwr_pin,
             m_stats.pwr_vout, m_stats.pwr_iout, m_stats.pwr_pout, m_stats.total_blocks_found,
             m_stats.duplicate_hashes, m_stats.last_ping_rtt);

    snprintf(url, sizeof(url), "%s:%d/api/v2/write?bucket=%s&org=%s&precision=s", m_host, m_port, m_bucket,
             m_org);

    ESP_LOGI(TAG, "URL: %s", url);
    ESP_LOGI(TAG, "POST: %s", m_big_buffer);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_header(client, "Authorization", m_auth_header);
    esp_http_client_set_header(client, "Content-Type", "text/plain");
    esp_http_client_set_post_field(client, m_big_buffer, strlen(m_big_buffer));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        int content_length = esp_http_client_get_content_length(client);

        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %d", status_code, content_length);

        if (status_code == 400) {
            int len = esp_http_client_read(client, m_big_buffer, m_big_buffer_SIZE - 1);
            if (len > 0) {
                m_big_buffer[len] = 0; // Null-terminate the response
                ESP_LOGE(TAG, "HTTP POST Error 400 Response: %s", m_big_buffer);
            } else {
                ESP_LOGE(TAG, "HTTP POST Error 400: No response body");
            }
        }
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

bool Influx::init(const char *host, int port, const char *token, const char *bucket, const char *org, const char *prefix)
{
    m_big_buffer = (char*) MALLOC(m_big_buffer_SIZE);

    if (!m_big_buffer) {
        ESP_LOGE(TAG, "error allocating influx message buffer");
        return false;
    }

    // zero stats
    memset(&m_stats, 0, sizeof(m_stats));

    m_port = port;
    m_host = strdup(host);
    m_token = strdup(token);
    m_bucket = strdup(bucket);
    m_prefix = strdup(prefix);
    m_org = strdup(org);
    m_lock = PTHREAD_MUTEX_INITIALIZER;

    // prepare auth header
    snprintf(m_auth_header, sizeof(m_auth_header), "Token %s", token);

    return true;
}
