#pragma once

#include "esp_http_server.h"

esp_err_t is_network_allowed(httpd_req_t * req);
esp_err_t set_cors_headers(httpd_req_t *req);