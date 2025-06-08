#pragma once

#include "esp_http_server.h"

esp_err_t PATCH_update_swarm(httpd_req_t *req);
esp_err_t GET_swarm(httpd_req_t *req);