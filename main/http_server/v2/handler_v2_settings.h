#pragma once
#include "esp_http_server.h"

esp_err_t GET_V2_settings(httpd_req_t *req);
esp_err_t PATCH_V2_settings(httpd_req_t *req);
