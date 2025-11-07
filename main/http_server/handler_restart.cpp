#include "esp_http_server.h"
#include "esp_log.h"

#include "global_state.h"
#include "http_cors.h"
#include "http_utils.h"

static const char *TAG = "http_restart";

extern bool enter_recovery;

esp_err_t POST_restart(httpd_req_t *req)
{
    // always set connection: close
    httpd_resp_set_hdr(req, "Connection", "close");

    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    // disable OTP when in recovery mode
    if (!enter_recovery && validateOTP(req) != ESP_OK) {
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Restarting System because of API Request");

    // Send HTTP response before restarting
    const char *resp_str = "System will restart shortly.";
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);

    // Delay to ensure the response is sent
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Restart the system
    POWER_MANAGEMENT_MODULE.restart();

    // unreachable
    return ESP_OK;
}
