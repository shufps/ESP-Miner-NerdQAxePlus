#include "esp_ota_ops.h"
#include "esp_http_server.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "cJSON.h"

#include "global_state.h"

#include "http_cors.h"
#include "http_utils.h"

static const char *TAG = "http_ota";

esp_err_t POST_WWW_update(httpd_req_t *req)
{
    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    int remaining = req->content_len;

    const esp_partition_t *www_partition =
        esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, "www");
    if (www_partition == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "WWW partition not found");
        return ESP_FAIL;
    }

    // Don't attempt to write more than what can be stored in the partition
    if (remaining > www_partition->size) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File provided is too large for device");
        return ESP_FAIL;
    }

    // Erase the entire www partition before writing
    {
        // lock the power management module
        LockGuard g(POWER_MANAGEMENT_MODULE);
        ESP_LOGI(TAG, "erasing www partition ...");
        ESP_ERROR_CHECK(esp_partition_erase_range(www_partition, 0, www_partition->size));
        ESP_LOGI(TAG, "erasing done");
    }

    // don't put it on the stack
    char *buf = (char*) malloc(2048);
    uint32_t offset = 0;

    while (remaining > 0) {
        int recv_len = httpd_req_recv(req, buf, min(remaining, 2048));

        if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;
        } else if (recv_len <= 0) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Protocol Error");
            free(buf);
            return ESP_FAIL;
        }

        {
            // lock the power management module
            LockGuard g(POWER_MANAGEMENT_MODULE);

            // print each 64kb
            if (!(offset & 0xffff)) {
                ESP_LOGI(TAG, "flashing to %08lx", offset);
            }
            if (esp_partition_write(www_partition, offset, (const void *) buf, recv_len) != ESP_OK) {
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Write Error");
                free(buf);
                return ESP_FAIL;
            }
        }
        remaining -= recv_len;
        offset += recv_len;

        // switch task to not stall the PID
        taskYIELD();
    }

    free(buf);

    httpd_resp_sendstr(req, "WWW update complete\n");
    return ESP_OK;
}

/*
 * Handle OTA file upload
 */
esp_err_t POST_OTA_update(httpd_req_t *req)
{
    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    esp_ota_handle_t ota_handle;
    int remaining = req->content_len;

    // lock the power management module
    LockGuard g(POWER_MANAGEMENT_MODULE);

    // Shut down buck converter before starting OTA.
    // During OTA, I2C conflicts prevent the PID from working correctly.
    // Disabling hardware avoids any risk of overheating.
    // The device will reboot after the update anyway.
    POWER_MANAGEMENT_MODULE.shutdown();

    const esp_partition_t *ota_partition = esp_ota_get_next_update_partition(NULL);
    ESP_ERROR_CHECK(esp_ota_begin(ota_partition, OTA_SIZE_UNKNOWN, &ota_handle));

    // don't put it on the stack
    char *buf = (char*) malloc(2048);
    uint32_t offset = 0;

    while (remaining > 0) {
        int recv_len = httpd_req_recv(req, buf, min(remaining, 2048));

        // Timeout Error: Just retry
        if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;

            // Serious Error: Abort OTA
        } else if (recv_len <= 0) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Protocol Error");
            free(buf);
            return ESP_FAIL;
        }

        // Successful Upload: Flash firmware chunk
        // print each 64kb
        if (!(offset & 0xffff)) {
            ESP_LOGI(TAG, "flashing to %08lx", offset);
        }
        if (esp_ota_write(ota_handle, (const void *) buf, recv_len) != ESP_OK) {
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Flash Error");
            free(buf);
            return ESP_FAIL;
        }

        remaining -= recv_len;
        offset += recv_len;

        // give the scheduler a chance for other tasks
        taskYIELD();
    }

    free(buf);

    // Validate and switch to new OTA image and reboot
    if (esp_ota_end(ota_handle) != ESP_OK || esp_ota_set_boot_partition(ota_partition) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Validation / Activation Error");
        return ESP_FAIL;
    }

    httpd_resp_sendstr(req, "Firmware update complete, rebooting now!\n");
    ESP_LOGI(TAG, "Restarting System because of Firmware update complete");
    vTaskDelay(pdMS_TO_TICKS(1000));
    POWER_MANAGEMENT_MODULE.restart();

    // unreachable
    return ESP_OK;
}

/*
 * Handle OTA update from GitHub URL
 * Expects JSON body: {"url": "https://github.com/...", "type": "firmware|www"}
 * type defaults to "firmware" if not specified
 */
esp_err_t POST_OTA_update_from_url(httpd_req_t *req)
{
    ESP_LOGI(TAG, "=== POST_OTA_update_from_url called ===");

    if (is_network_allowed(req) != ESP_OK) {
        ESP_LOGE(TAG, "Network not allowed");
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    ESP_LOGI(TAG, "Network allowed, content_len=%d", req->content_len);

    // Read JSON body
    char *buf = (char*)malloc(req->content_len + 1);
    if (buf == NULL) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
    }

    int ret = httpd_req_recv(req, buf, req->content_len);
    if (ret <= 0) {
        ESP_LOGE(TAG, "Failed to read request body, ret=%d", ret);
        free(buf);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read request body");
    }
    buf[ret] = '\0';
    ESP_LOGI(TAG, "Received body: %s", buf);

    // Parse JSON
    cJSON *json = cJSON_Parse(buf);
    free(buf);

    if (json == NULL) {
        ESP_LOGE(TAG, "Invalid JSON");
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
    }

    cJSON *url_item = cJSON_GetObjectItem(json, "url");
    if (url_item == NULL || !cJSON_IsString(url_item)) {
        ESP_LOGE(TAG, "Missing 'url' field");
        cJSON_Delete(json);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing 'url' field");
    }

    // Get update type (defaults to "firmware")
    cJSON *type_item = cJSON_GetObjectItem(json, "type");
    bool is_www_update = false;
    if (type_item != NULL && cJSON_IsString(type_item)) {
        if (strcmp(type_item->valuestring, "www") == 0) {
            is_www_update = true;
        }
    }

    const char *url = url_item->valuestring;
    ESP_LOGI(TAG, "Starting %s update from GitHub URL: %s", is_www_update ? "WWW" : "firmware", url);

    esp_err_t err = ESP_OK;

    if (is_www_update) {
        // WWW partition update - Using esp_https_ota (handles long GitHub CDN URLs better)
        ESP_LOGI(TAG, "OTADEBUG === Starting WWW Partition Streaming Update ===");

        const esp_partition_t *www_partition =
            esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, "www");
        if (www_partition == NULL) {
            ESP_LOGE(TAG, "OTADEBUG WWW partition not found!");
            cJSON_Delete(json);
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "WWW partition not found");
        }

        ESP_LOGI(TAG, "OTADEBUG WWW partition found: size=%d bytes", (int)www_partition->size);
        ESP_LOGI(TAG, "OTADEBUG Download URL: %s", url);

        // Allocate small chunk buffer (8KB) - NO large PSRAM buffer
        const size_t chunk_size = 8192;
        char *chunk_buffer = (char*)malloc(chunk_size);
        if (chunk_buffer == NULL) {
            ESP_LOGE(TAG, "OTADEBUG Failed to allocate %d byte chunk buffer", (int)chunk_size);
            cJSON_Delete(json);
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
        }
        ESP_LOGI(TAG, "OTADEBUG Allocated %d byte chunk buffer for streaming", (int)chunk_size);

        // STEP 1 & 2: Manual redirect following with event handler to capture Location header
        ESP_LOGI(TAG, "OTADEBUG === STEP 1: Following redirects manually ===");
        ESP_LOGI(TAG, "OTA_PROGRESS: 0%% (connecting)");

        // Context for capturing Location header via event handler
        struct {
            char location_url[1024];  // Buffer for Location header (up to 1KB)
            bool location_found;
            int location_truncated;
        } redirect_ctx = {
            .location_url = {0},
            .location_found = false,
            .location_truncated = 0
        };

        // Event handler to capture Location header (bypasses buffer size limits)
        auto header_event_handler = [](esp_http_client_event_t *evt) -> esp_err_t {
            auto *ctx = (decltype(redirect_ctx)*)evt->user_data;

            if (evt->event_id == HTTP_EVENT_ON_HEADER) {
                if (strcasecmp(evt->header_key, "Location") == 0) {
                    int len = strlen(evt->header_value);
                    ESP_LOGI(TAG, "OTADEBUG Location header found: length=%d", len);

                    if (len < (int)sizeof(ctx->location_url)) {
                        strncpy(ctx->location_url, evt->header_value, sizeof(ctx->location_url) - 1);
                        ctx->location_url[sizeof(ctx->location_url) - 1] = '\0';
                        ctx->location_found = true;
                        ESP_LOGI(TAG, "OTADEBUG Location captured (first 100 chars): %.100s...", ctx->location_url);
                    } else {
                        ESP_LOGE(TAG, "OTADEBUG Location header too long: %d > %d", len, (int)sizeof(ctx->location_url));
                        ctx->location_truncated = len;
                    }
                }
            }
            return ESP_OK;
        };

        int status_code = 0;
        int content_length = 0;
        int redirect_count = 0;
        const int max_redirects = 5;
        char *current_url = strdup(url);  // Start with original URL
        esp_http_client_handle_t client = NULL;

        if (current_url == NULL) {
            ESP_LOGE(TAG, "OTADEBUG Failed to allocate URL buffer");
            free(chunk_buffer);
            cJSON_Delete(json);
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
        }

        while (redirect_count <= max_redirects) {
            ESP_LOGI(TAG, "OTADEBUG Redirect %d: Connecting to: %.100s...", redirect_count, current_url);

            // Reset redirect context
            redirect_ctx.location_found = false;
            redirect_ctx.location_truncated = 0;
            memset(redirect_ctx.location_url, 0, sizeof(redirect_ctx.location_url));

            // Cleanup previous client if exists
            if (client != NULL) {
                esp_http_client_cleanup(client);
                client = NULL;
            }

            // Create new client with current URL and event handler
            esp_http_client_config_t config = {};
            config.url = current_url;
            config.timeout_ms = 60000;
            config.crt_bundle_attach = esp_crt_bundle_attach;
            config.buffer_size = 16384;  // 16KB buffer
            config.event_handler = header_event_handler;
            config.user_data = &redirect_ctx;
            config.disable_auto_redirect = true;  // Manual redirect handling

            client = esp_http_client_init(&config);
            if (client == NULL) {
                ESP_LOGE(TAG, "OTADEBUG HTTP client init failed (redirect %d)", redirect_count);
                free(current_url);
                free(chunk_buffer);
                cJSON_Delete(json);
                return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "HTTP client init failed");
            }

            // Set User-Agent
            esp_http_client_set_header(client, "User-Agent", "ESP32-Miner/1.0");

            // Open connection
            err = esp_http_client_open(client, 0);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "OTADEBUG Failed to open connection (redirect %d): %s", redirect_count, esp_err_to_name(err));

                // Check if this is likely due to URL length (GitHub CDN URLs can be 800+ bytes)
                if (redirect_count > 0 && redirect_ctx.location_found) {
                    int url_len = strlen(current_url);
                    ESP_LOGE(TAG, "╔════════════════════════════════════════════════════════════════╗");
                    ESP_LOGE(TAG, "║ ERREUR: URL GitHub CDN trop longue pour ESP32                 ║");
                    ESP_LOGE(TAG, "║ URL length: %d bytes (ESP32 limit: ~512 bytes)              ║", url_len);
                    ESP_LOGE(TAG, "║                                                                ║");
                    ESP_LOGE(TAG, "║ SOLUTIONS:                                                     ║");
                    ESP_LOGE(TAG, "║ 1. Télécharger www.bin manuellement depuis:                   ║");
                    ESP_LOGE(TAG, "║    https://github.com/.../releases/latest                     ║");
                    ESP_LOGE(TAG, "║    puis uploader via l'interface web                          ║");
                    ESP_LOGE(TAG, "║                                                                ║");
                    ESP_LOGE(TAG, "║ 2. Héberger www.bin sur un serveur avec URLs courtes         ║");
                    ESP_LOGE(TAG, "║                                                                ║");
                    ESP_LOGE(TAG, "║ Note: Les URLs GitHub CDN contiennent des tokens              ║");
                    ESP_LOGE(TAG, "║       d'authentification très longs (incompatible ESP32)      ║");
                    ESP_LOGE(TAG, "╚════════════════════════════════════════════════════════════════╝");
                }

                free(current_url);
                esp_http_client_cleanup(client);
                free(chunk_buffer);
                cJSON_Delete(json);
                return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "GitHub CDN URL too long for ESP32");
            }

            // Fetch headers (this triggers event handler)
            content_length = esp_http_client_fetch_headers(client);
            status_code = esp_http_client_get_status_code(client);

            ESP_LOGI(TAG, "OTADEBUG Redirect %d: Status=%d, Content-Length=%d", redirect_count, status_code, content_length);

            // Check if we got final response (200) or another redirect (301, 302, 303, 307, 308)
            if (status_code == 200) {
                ESP_LOGI(TAG, "OTADEBUG Got 200 OK after %d redirect(s)", redirect_count);
                free(current_url);
                break;  // Success! Exit loop and proceed to download (client stays open)
            } else if (status_code == 301 || status_code == 302 || status_code == 303 || status_code == 307 || status_code == 308) {
                // Check if Location was captured by event handler
                if (redirect_ctx.location_truncated > 0) {
                    ESP_LOGE(TAG, "OTADEBUG Redirect URL too long: %d bytes (max %d)", redirect_ctx.location_truncated, (int)sizeof(redirect_ctx.location_url));
                    free(current_url);
                    esp_http_client_close(client);
                    esp_http_client_cleanup(client);
                    free(chunk_buffer);
                    cJSON_Delete(json);
                    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Redirect URL too long");
                }

                if (redirect_ctx.location_found) {
                    ESP_LOGI(TAG, "OTADEBUG Redirect %d: Following to next URL", redirect_count);

                    // Close current connection (will recreate client in next iteration)
                    esp_http_client_close(client);

                    // Check if URL is too long for ESP32 - use URL shortener (is.gd with POST)
                    int url_len = strlen(redirect_ctx.location_url);
                    if (url_len > 500) {
                        ESP_LOGW(TAG, "OTADEBUG URL too long (%d bytes), using is.gd URL shortener...", url_len);

                        // Use is.gd API with POST (long URL in body, not in URL!)
                        // POST to https://is.gd/create.php with form data: url=LONG_URL&format=simple
                        char post_data[1200];
                        snprintf(post_data, sizeof(post_data), "url=%s&format=simple", redirect_ctx.location_url);

                        // Create temp client for is.gd request
                        esp_http_client_config_t shortener_config = {};
                        shortener_config.url = "https://is.gd/create.php";
                        shortener_config.timeout_ms = 15000;
                        shortener_config.method = HTTP_METHOD_POST;
                        shortener_config.buffer_size = 512;
                        shortener_config.crt_bundle_attach = esp_crt_bundle_attach;

                        esp_http_client_handle_t shortener_client = esp_http_client_init(&shortener_config);
                        if (shortener_client == NULL) {
                            ESP_LOGE(TAG, "OTADEBUG Failed to init URL shortener client");
                            free(current_url);
                            esp_http_client_cleanup(client);
                            free(chunk_buffer);
                            cJSON_Delete(json);
                            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "URL shortener init failed");
                        }

                        // Set POST headers and data
                        esp_http_client_set_header(shortener_client, "Content-Type", "application/x-www-form-urlencoded");
                        esp_http_client_set_post_field(shortener_client, post_data, strlen(post_data));

                        // Get shortened URL
                        char short_url[256] = {0};
                        err = esp_http_client_open(shortener_client, strlen(post_data));
                        if (err == ESP_OK) {
                            esp_http_client_write(shortener_client, post_data, strlen(post_data));
                            esp_http_client_fetch_headers(shortener_client);
                            int len = esp_http_client_read(shortener_client, short_url, sizeof(short_url) - 1);
                            if (len > 0 && len < 100) {  // is.gd returns very short URLs
                                short_url[len] = '\0';
                                // Remove any trailing newline
                                char *newline = strchr(short_url, '\n');
                                if (newline) *newline = '\0';

                                ESP_LOGI(TAG, "OTADEBUG ✓ URL shortened: %d → %d bytes", url_len, (int)strlen(short_url));
                                ESP_LOGI(TAG, "OTADEBUG ✓ Short URL: %s", short_url);

                                // Use shortened URL
                                free(current_url);
                                current_url = strdup(short_url);
                            } else {
                                ESP_LOGW(TAG, "OTADEBUG URL shortener returned invalid response (len=%d)", len);
                                free(current_url);
                                current_url = strdup(redirect_ctx.location_url);
                            }
                        } else {
                            ESP_LOGW(TAG, "OTADEBUG URL shortener request failed: %s", esp_err_to_name(err));
                            free(current_url);
                            current_url = strdup(redirect_ctx.location_url);
                        }

                        esp_http_client_close(shortener_client);
                        esp_http_client_cleanup(shortener_client);
                    } else {
                        // URL is short enough, use as-is
                        free(current_url);
                        current_url = strdup(redirect_ctx.location_url);
                    }

                    if (current_url == NULL) {
                        ESP_LOGE(TAG, "OTADEBUG Failed to allocate redirect URL");
                        esp_http_client_cleanup(client);
                        free(chunk_buffer);
                        cJSON_Delete(json);
                        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
                    }

                    redirect_count++;
                    continue;  // Next iteration
                } else {
                    ESP_LOGE(TAG, "OTADEBUG Redirect %d: No Location header found for status %d", redirect_count, status_code);
                    free(current_url);
                    esp_http_client_close(client);
                    esp_http_client_cleanup(client);
                    free(chunk_buffer);
                    cJSON_Delete(json);
                    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Redirect without Location");
                }
            } else {
                ESP_LOGE(TAG, "OTADEBUG Unexpected HTTP status: %d", status_code);
                free(current_url);
                esp_http_client_close(client);
                esp_http_client_cleanup(client);
                free(chunk_buffer);
                cJSON_Delete(json);
                return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid HTTP status");
            }
        }

        // Check if we exceeded max redirects
        if (redirect_count > max_redirects) {
            ESP_LOGE(TAG, "OTADEBUG Too many redirects (>%d)", max_redirects);
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            free(chunk_buffer);
            cJSON_Delete(json);
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Too many redirects");
        }

        // Verify we have valid response
        if (status_code != 200 || client == NULL) {
            ESP_LOGE(TAG, "OTADEBUG Final status is not 200: %d", status_code);
            if (client) {
                esp_http_client_close(client);
                esp_http_client_cleanup(client);
            }
            free(chunk_buffer);
            cJSON_Delete(json);
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid final status");
        }

        if (content_length <= 0) {
            ESP_LOGE(TAG, "OTADEBUG Invalid content length: %d", content_length);
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            free(chunk_buffer);
            cJSON_Delete(json);
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid content length");
        }

        if (content_length > (int)www_partition->size) {
            ESP_LOGE(TAG, "OTADEBUG File too large: %d > %d", content_length, (int)www_partition->size);
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            free(chunk_buffer);
            cJSON_Delete(json);
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File too large");
        }

        ESP_LOGI(TAG, "OTADEBUG Headers valid, ready to stream %d bytes", content_length);

        // STEP 3: Erase partition BEFORE streaming
        ESP_LOGI(TAG, "OTADEBUG === STEP 3: Erasing partition ===");
        ESP_LOGI(TAG, "OTA_PROGRESS: 5%% (erasing)");

        {
            // Short lock during erase to prevent PID stall
            LockGuard g(POWER_MANAGEMENT_MODULE);
            err = esp_partition_erase_range(www_partition, 0, www_partition->size);
        }
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "OTADEBUG Partition erase failed: %s", esp_err_to_name(err));
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            free(chunk_buffer);
            cJSON_Delete(json);
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Erase failed");
        }
        ESP_LOGI(TAG, "OTADEBUG Partition erased successfully");

        // STEP 4: Stream download directly to partition
        ESP_LOGI(TAG, "OTADEBUG === STEP 4: Streaming download to partition ===");
        ESP_LOGI(TAG, "OTA_PROGRESS: 10%% (streaming)");

        int total_read = 0;
        int last_progress = 10;
        int last_log_bytes = 0;

        while (total_read < content_length) {
            // Read chunk from HTTP stream
            int read_len = esp_http_client_read(client, chunk_buffer, chunk_size);

            if (read_len < 0) {
                ESP_LOGE(TAG, "OTADEBUG HTTP read error at offset %d: %s", total_read, esp_err_to_name(read_len));
                esp_http_client_close(client);
                esp_http_client_cleanup(client);
                free(chunk_buffer);
                cJSON_Delete(json);
                return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Download failed");
            }

            if (read_len == 0) {
                ESP_LOGW(TAG, "OTADEBUG No more data from HTTP client (read=%d, expected=%d)", total_read, content_length);
                break;
            }

            // Write chunk directly to partition
            {
                // Short lock during write to prevent PID stall
                LockGuard g(POWER_MANAGEMENT_MODULE);
                err = esp_partition_write(www_partition, total_read, chunk_buffer, read_len);
            }
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "OTADEBUG Partition write failed at offset %d: %s", total_read, esp_err_to_name(err));
                esp_http_client_close(client);
                esp_http_client_cleanup(client);
                free(chunk_buffer);
                cJSON_Delete(json);
                return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Write failed");
            }

            total_read += read_len;

            // Progress logging every 200KB
            if (total_read - last_log_bytes >= 204800) {  // 200KB
                ESP_LOGI(TAG, "OTADEBUG Streamed %d / %d bytes (%.1f%%)",
                         total_read, content_length, (total_read * 100.0) / content_length);
                last_log_bytes = total_read;
            }

            // Progress updates every 5%
            int progress = 10 + ((total_read * 85) / content_length);  // 10-95% for streaming
            if (progress >= last_progress + 5) {
                ESP_LOGI(TAG, "OTA_PROGRESS: %d%% (streaming)", progress);
                last_progress = progress;
            }

            taskYIELD();  // Allow other tasks to run
        }

        ESP_LOGI(TAG, "OTADEBUG === STEP 5: Finalizing ===");

        // Close HTTP connection
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        free(chunk_buffer);

        // Verify download completion
        if (total_read != content_length) {
            ESP_LOGE(TAG, "OTADEBUG Incomplete download: %d/%d bytes", total_read, content_length);
            cJSON_Delete(json);
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Incomplete download");
        }

        ESP_LOGI(TAG, "OTADEBUG Streaming complete: %d bytes written to www partition", total_read);
        ESP_LOGI(TAG, "OTA_PROGRESS: 100%%");
        ESP_LOGI(TAG, "OTADEBUG === WWW Update Successful ===");

        httpd_resp_sendstr(req, "OK");
        cJSON_Delete(json);

        // Restart after response
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP_LOGI(TAG, "Rebooting...");
        POWER_MANAGEMENT_MODULE.restart();

    } else {
        // Firmware update - use esp_https_ota
        // Lock power management and shutdown hardware for firmware OTA
        LockGuard g(POWER_MANAGEMENT_MODULE);
        POWER_MANAGEMENT_MODULE.shutdown();

        // browser_download_url redirects to CDN
        esp_http_client_config_t config = {};
        config.url = url;
        config.timeout_ms = 60000;  // 60 second timeout
        config.crt_bundle_attach = esp_crt_bundle_attach;
        config.keep_alive_enable = true;
        config.buffer_size = 4096;  // 4KB buffer
        config.buffer_size_tx = 4096;
        config.max_redirection_count = 2;  // GitHub -> CDN redirect

        esp_https_ota_config_t ota_config = {};
        ota_config.http_config = &config;

        ESP_LOGI(TAG, "OTA_PROGRESS: 0%%");
        ESP_LOGI(TAG, "Starting esp_https_ota...");

        esp_https_ota_handle_t https_ota_handle = NULL;
        err = esp_https_ota_begin(&ota_config, &https_ota_handle);

        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_https_ota_begin failed: %s", esp_err_to_name(err));
            cJSON_Delete(json);
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA begin failed");
        }

        esp_app_desc_t app_desc;
        err = esp_https_ota_get_img_desc(https_ota_handle, &app_desc);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_https_ota_get_img_desc failed");
            esp_https_ota_abort(https_ota_handle);
            cJSON_Delete(json);
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to get image descriptor");
        }

        int total_size = esp_https_ota_get_image_size(https_ota_handle);
        int downloaded = 0;
        int last_progress = 0;

        while (1) {
            err = esp_https_ota_perform(https_ota_handle);
            if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
                break;
            }

            downloaded = esp_https_ota_get_image_len_read(https_ota_handle);
            int progress = (downloaded * 100) / total_size;

            // Log progress every 5% to avoid flooding
            if (progress >= last_progress + 5 || progress == 100) {
                ESP_LOGI(TAG, "OTA_PROGRESS: %d%%", progress);
                last_progress = progress;
            }

            taskYIELD();
        }

        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_https_ota_perform failed: %s", esp_err_to_name(err));
            esp_https_ota_abort(https_ota_handle);
            cJSON_Delete(json);
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA download failed");
        }

        err = esp_https_ota_finish(https_ota_handle);

        cJSON_Delete(json);

        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_https_ota_finish failed: %s", esp_err_to_name(err));
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA finish failed");
        }

        ESP_LOGI(TAG, "OTA_PROGRESS: 100%%");
        ESP_LOGI(TAG, "OTA update from GitHub successful!");
        httpd_resp_sendstr(req, "OK");

        // Restart after response
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP_LOGI(TAG, "Rebooting...");
        POWER_MANAGEMENT_MODULE.restart();
    }

    return ESP_OK;
}
