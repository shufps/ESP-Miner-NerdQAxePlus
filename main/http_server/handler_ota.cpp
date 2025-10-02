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
 * Expects JSON body: {"url": "https://github.com/..."}
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

    const char *url = url_item->valuestring;
    ESP_LOGI(TAG, "Starting OTA update from GitHub URL: %s", url);

    // Lock power management and shutdown hardware BEFORE OTA
    LockGuard g(POWER_MANAGEMENT_MODULE);
    POWER_MANAGEMENT_MODULE.shutdown();

    // Use esp_https_ota which handles redirects automatically
    esp_http_client_config_t config = {};
    config.url = url;
    config.timeout_ms = 60000;  // 60 second timeout
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.keep_alive_enable = true;
    config.buffer_size = 8192;  // Increase buffer for large files
    config.buffer_size_tx = 4096;

    esp_https_ota_config_t ota_config = {};
    ota_config.http_config = &config;

    ESP_LOGI(TAG, "OTA_PROGRESS: 0%%");
    ESP_LOGI(TAG, "Starting esp_https_ota...");

    esp_https_ota_handle_t https_ota_handle = NULL;
    esp_err_t err = esp_https_ota_begin(&ota_config, &https_ota_handle);

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

    return ESP_OK;
}
