#include "esp_ota_ops.h"
#include "esp_http_server.h"
#include "esp_log.h"

#include "global_state.h"

#include "http_cors.h"
#include "http_utils.h"

static const char *TAG = "http_ota";

extern bool enter_recovery;

// a client that silently drops off (e.g. WiFi lost mid-upload) only produces
// receive timeouts; give up after this many in a row instead of blocking the
// (single) httpd task forever
#define MAX_RECV_TIMEOUTS 6


esp_err_t POST_WWW_update(httpd_req_t *req)
{
    // close connection when out of scope
    ConGuard g(http_server, req);

    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    // disable OTP when in recovery mode
    if (!enter_recovery && validateOTP(req) != ESP_OK) {
        return ESP_FAIL;
    }

    if (req->content_len == 0) {
        // would erase the partition and report success without writing anything
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty upload");
        return ESP_FAIL;
    }
    size_t remaining = req->content_len;

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
    if (!buf) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return ESP_FAIL;
    }
    uint32_t offset = 0;
    int timeouts = 0;

    while (remaining > 0) {
        int recv_len = httpd_req_recv(req, buf, min(remaining, (size_t) 2048));

        if (recv_len == HTTPD_SOCK_ERR_TIMEOUT && ++timeouts < MAX_RECV_TIMEOUTS) {
            continue;
        } else if (recv_len <= 0) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Protocol Error");
            free(buf);
            return ESP_FAIL;
        }
        timeouts = 0;

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
    // close connection when out of scope
    ConGuard g(http_server, req);

    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    if (validateOTP(req) != ESP_OK) {
        return ESP_FAIL;
    }

    esp_ota_handle_t ota_handle = 0;

    if (req->content_len == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty upload");
        return ESP_FAIL;
    }
    size_t remaining = req->content_len;

    // don't put it on the stack
    char *buf = (char*) malloc(2048);
    if (!buf) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return ESP_FAIL;
    }

    // lock the power management module
    LockGuard lg(POWER_MANAGEMENT_MODULE);

    // Shut down buck converter before starting OTA.
    // During OTA, I2C conflicts prevent the PID from working correctly.
    // Disabling hardware avoids any risk of overheating.
    // The device will reboot after the update anyway.
    POWER_MANAGEMENT_MODULE.shutdown();

    // The board is shut down from here on: every failure has to end in a
    // restart (into the old firmware), otherwise the miner stays off.
    auto fail = [&](const char *msg) {
        if (ota_handle) {
            esp_ota_abort(ota_handle);
        }
        free(buf);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, msg);
        ESP_LOGE(TAG, "firmware update failed (%s), restarting", msg);
        vTaskDelay(pdMS_TO_TICKS(1000));
        POWER_MANAGEMENT_MODULE.restart();
        return ESP_FAIL; // unreachable
    };

    const esp_partition_t *ota_partition = esp_ota_get_next_update_partition(NULL);
    if (!ota_partition || esp_ota_begin(ota_partition, OTA_SIZE_UNKNOWN, &ota_handle) != ESP_OK) {
        ota_handle = 0;
        return fail("OTA begin failed");
    }

    uint32_t offset = 0;
    int timeouts = 0;

    while (remaining > 0) {
        int recv_len = httpd_req_recv(req, buf, min(remaining, (size_t) 2048));

        // Timeout Error: retry a few times
        if (recv_len == HTTPD_SOCK_ERR_TIMEOUT && ++timeouts < MAX_RECV_TIMEOUTS) {
            continue;

            // Serious Error: Abort OTA
        } else if (recv_len <= 0) {
            return fail("Protocol Error");
        }
        timeouts = 0;

        // Successful Upload: Flash firmware chunk
        // print each 64kb
        if (!(offset & 0xffff)) {
            ESP_LOGI(TAG, "flashing to %08lx", offset);
        }
        if (esp_ota_write(ota_handle, (const void *) buf, recv_len) != ESP_OK) {
            return fail("Flash Error");
        }

        remaining -= recv_len;
        offset += recv_len;

        // give the scheduler a chance for other tasks
        taskYIELD();
    }

    // Validate and switch to new OTA image and reboot
    // (esp_ota_end() frees the handle, also when it fails)
    esp_err_t end_err = esp_ota_end(ota_handle);
    ota_handle = 0;
    if (end_err != ESP_OK || esp_ota_set_boot_partition(ota_partition) != ESP_OK) {
        return fail("Validation / Activation Error");
    }
    free(buf);

    httpd_resp_sendstr(req, "Firmware update complete, rebooting now!\n");
    ESP_LOGI(TAG, "Restarting System because of Firmware update complete");
    vTaskDelay(pdMS_TO_TICKS(1000));
    POWER_MANAGEMENT_MODULE.restart();

    // unreachable
    return ESP_OK;
}
