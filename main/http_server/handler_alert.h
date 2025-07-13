#pragma once

#include "esp_http_client.h"
#include "esp_log.h"

esp_err_t GET_alert_info(httpd_req_t *req);
esp_err_t POST_update_alert(httpd_req_t *req);
esp_err_t POST_test_alert(httpd_req_t *req);
