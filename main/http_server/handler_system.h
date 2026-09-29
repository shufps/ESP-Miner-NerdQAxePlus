#pragma once

#include "sdkconfig.h"

esp_err_t GET_system_info(httpd_req_t *req);
esp_err_t PATCH_update_settings(httpd_req_t *req);

esp_err_t GET_system_asic(httpd_req_t *req);
#if CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH
esp_err_t GET_system_coredump(httpd_req_t *req);
#endif
esp_err_t POST_reset_stats(httpd_req_t *req);
