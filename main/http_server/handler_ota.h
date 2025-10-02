#pragma once

#include "esp_http_server.h"

esp_err_t POST_WWW_update(httpd_req_t *req);
esp_err_t POST_OTA_update(httpd_req_t *req);
esp_err_t POST_OTA_update_from_url(httpd_req_t *req);