#pragma once

#include "global_state.h"
#include "esp_vfs.h"
#include "esp_http_server.h"
#include "ArduinoJson.h"
#include "../otp/otp.h"

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + 128)
#define SCRATCH_BUFSIZE (16384)

typedef struct
{
    char base_path[ESP_VFS_PATH_MAX + 1];
    char scratch[SCRATCH_BUFSIZE];
} rest_server_context_t;

#define REST_CHECK(a, str, goto_tag, ...)                                                                                          \
    do {                                                                                                                           \
        if (!(a)) {                                                                                                                \
            ESP_LOGE(TAG, "%s(%d): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__);                                                  \
            goto goto_tag;                                                                                                         \
        }                                                                                                                          \
    } while (0)

#define MESSAGE_QUEUE_SIZE (128)

#define CHECK_FILE_EXTENSION(filename, ext) (strcasecmp(&filename[strlen(filename) - strlen(ext)], ext) == 0)

#define max(a,b) ((a)>(b))?(a):(b)
#define min(a,b) ((a)<(b))?(a):(b)

esp_err_t sendJsonResponse(httpd_req_t *req, JsonDocument &doc);
esp_err_t getPostData(httpd_req_t *req);
esp_err_t getJsonData(httpd_req_t *req, JsonDocument &doc);
esp_err_t validateOTP(httpd_req_t *req, bool force = false);

extern httpd_handle_t http_server;

class ConGuard {
protected:
    httpd_handle_t m_http_server;
    httpd_req_t *m_req;
public:
    ConGuard(httpd_handle_t http_server, httpd_req_t *req): m_http_server(http_server), m_req(req) {
        httpd_resp_set_hdr(m_req, "Connection", "close");
    };
    ~ConGuard() {
        int sock = httpd_req_to_sockfd(m_req);
        if (sock >= 0 && m_http_server) {
            httpd_sess_trigger_close(m_http_server, sock);
        }
    }
};