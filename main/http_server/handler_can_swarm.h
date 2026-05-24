#pragma once

#include "esp_http_server.h"

esp_err_t GET_can_slaves(httpd_req_t *req);
esp_err_t PATCH_can_slave(httpd_req_t *req);
esp_err_t DELETE_can_slave(httpd_req_t *req);
