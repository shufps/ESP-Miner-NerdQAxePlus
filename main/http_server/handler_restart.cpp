#include "esp_http_server.h"
#include "esp_log.h"

#include "global_state.h"
#include "http_cors.h"
#include "http_utils.h"

static const char *TAG = "http_restart";

esp_err_t POST_restart(httpd_req_t *req)
{
    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    ESP_LOGI(TAG, "Restarting System because of API Request");

    // Send HTTP response before restarting
    const char *resp_str = "System will restart shortly.";
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);

    // Delay to ensure the response is sent
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    // Restart the system
    POWER_MANAGEMENT_MODULE.restart();

    // This return statement will never be reached, but it's good practice to include it
    return ESP_OK;
}
