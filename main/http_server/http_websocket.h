#pragma once

#include "esp_http_server.h"

esp_err_t echo_handler(httpd_req_t *req);
void websocket_log_handler(void* param);

void websocket_start();
// Stops log forwarding if fd is (still) the active log websocket. Called when a
// socket closes; a newer websocket on another fd is left alone.
void websocket_close_fd(int fd);
