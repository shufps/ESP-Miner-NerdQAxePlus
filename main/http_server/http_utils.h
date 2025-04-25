#pragma once

#include "esp_vfs.h"
#include "esp_http_server.h"
#include "ArduinoJson.h"

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

#ifdef CONFIG_SPIRAM
#define ALLOC(s) heap_caps_malloc(s, MALLOC_CAP_SPIRAM)
#define CALLOC(s, t) heap_caps_calloc(s, t, MALLOC_CAP_SPIRAM)
#define FREE(p) do { if (p) { heap_caps_free(p); (p) = NULL; } } while (0)
#else
#define ALLOC(s) malloc(s)
#define CALLOC(s, t) calloc(s, t)
#define FREE(p) do { if (p) { free(p); (p) = NULL; } } while (0)
#endif

#define MESSAGE_QUEUE_SIZE (128)

#define CHECK_FILE_EXTENSION(filename, ext) (strcasecmp(&filename[strlen(filename) - strlen(ext)], ext) == 0)

#define max(a,b) ((a)>(b))?(a):(b)
#define min(a,b) ((a)<(b))?(a):(b)

esp_err_t sendJsonResponse(httpd_req_t *req, JsonDocument &doc);