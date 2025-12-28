#include "esp_http_server.h"
#include "esp_log.h"

#include "global_state.h"
#include "http_cors.h"
#include "http_utils.h"
#include "ui_ipc.h"

static const char *TAG = "http_shutdown";

extern bool enter_recovery;

esp_err_t POST_shutdown(httpd_req_t *req)
{
    // close connection when out of scope
    ConGuard g(http_server, req);

    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }
/*
    if (validateOTP(req) != ESP_OK) {
        return ESP_FAIL;
    }
*/
    ESP_LOGI(TAG, "Shutting down System because of API Request");

    // Send HTTP response before shutting down
    const char *resp_str = "System will shutdown shortly.";
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);

    // Delay to ensure the response is sent
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Shutdown the system
    POWER_MANAGEMENT_MODULE.shutdown();

    // unreachable
    return ESP_OK;
}
