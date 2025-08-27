#include "esp_ota_ops.h"
#include "esp_http_server.h"
#include "esp_log.h"

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

    {
        // lock the power management module
        LockGuard g(POWER_MANAGEMENT_MODULE);

        // Erase the entire www partition before writing
        ESP_ERROR_CHECK(esp_partition_erase_range(www_partition, 0, www_partition->size));

        // don't put it on the stack
        char *buf = (char*) malloc(2048);

        while (remaining > 0) {
            int recv_len = httpd_req_recv(req, buf, min(remaining, 2048));

            if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            } else if (recv_len <= 0) {
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Protocol Error");
                free(buf);
                return ESP_FAIL;
            }

            if (esp_partition_write(www_partition, www_partition->size - remaining, (const void *) buf, recv_len) != ESP_OK) {
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Write Error");
                free(buf);
                return ESP_FAIL;
            }

            remaining -= recv_len;

            // give the scheduler a chance for other tasks
            vTaskDelay(1);
        }

        free(buf);
    }

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

    {
        // lock the power management module
        LockGuard g(POWER_MANAGEMENT_MODULE);

        const esp_partition_t *ota_partition = esp_ota_get_next_update_partition(NULL);
        ESP_ERROR_CHECK(esp_ota_begin(ota_partition, OTA_SIZE_UNKNOWN, &ota_handle));

        // don't put it on the stack
        char *buf = (char*) malloc(2048);

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
            if (esp_ota_write(ota_handle, (const void *) buf, recv_len) != ESP_OK) {
                esp_ota_abort(ota_handle);
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Flash Error");
                free(buf);
                return ESP_FAIL;
            }

            remaining -= recv_len;

            // give the scheduler a chance for other tasks
            vTaskDelay(1);
        }

        free(buf);

        // Validate and switch to new OTA image and reboot
        if (esp_ota_end(ota_handle) != ESP_OK || esp_ota_set_boot_partition(ota_partition) != ESP_OK) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Validation / Activation Error");
            return ESP_FAIL;
        }
    }

    httpd_resp_sendstr(req, "Firmware update complete, rebooting now!\n");
    ESP_LOGI(TAG, "Restarting System because of Firmware update complete");
    vTaskDelay(pdMS_TO_TICKS(1000));
    POWER_MANAGEMENT_MODULE.restart();

    return ESP_OK;
}
