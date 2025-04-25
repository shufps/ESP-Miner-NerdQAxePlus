#pragma once

#include "esp_http_server.h"

esp_err_t GET_influx_info(httpd_req_t *req);
esp_err_t PATCH_update_influx(httpd_req_t *req);