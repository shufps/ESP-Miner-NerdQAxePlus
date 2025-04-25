#pragma once

#include "esp_http_server.h"

esp_err_t echo_handler(httpd_req_t *req);
void websocket_log_handler(void* param);

void websocket_start();