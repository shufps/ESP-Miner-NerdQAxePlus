#pragma once

#include "esp_http_server.h"

esp_err_t rest_common_get_handler(httpd_req_t *req);
esp_err_t init_fs(void);