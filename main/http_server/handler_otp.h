#pragma once

esp_err_t POST_create_otp(httpd_req_t *req);
esp_err_t PATCH_update_otp(httpd_req_t *req);
esp_err_t POST_create_otp_session(httpd_req_t *req);
esp_err_t GET_otp_status(httpd_req_t *req);