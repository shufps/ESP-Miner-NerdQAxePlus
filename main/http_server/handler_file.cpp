#include <fcntl.h>
#include <string.h>

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"

#include "http_cors.h"
#include "http_utils.h"
#include "guards.h"

static const char* TAG="http_file";

#define CACHE_POLICY_NO_CACHE    "no-cache"
#define CACHE_POLICY_CACHE       "max-age=2592000"
#define CACHE_POLICY_IMMUTABLE   "public, max-age=31536000, immutable"

esp_err_t init_fs(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = false
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ESP_FAIL;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
    return ESP_OK;
}

/* Set HTTP response content type according to file extension */
static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filepath)
{
    const char *type = "text/plain";
    if (CHECK_FILE_EXTENSION(filepath, ".html")) {
        type = "text/html";
    } else if (CHECK_FILE_EXTENSION(filepath, ".js")) {
        type = "application/javascript";
    } else if (CHECK_FILE_EXTENSION(filepath, ".css")) {
        type = "text/css";
    } else if (CHECK_FILE_EXTENSION(filepath, ".png")) {
        type = "image/png";
    } else if (CHECK_FILE_EXTENSION(filepath, ".ico")) {
        type = "image/x-icon";
    } else if (CHECK_FILE_EXTENSION(filepath, ".svg")) {
        type = "image/svg+xml";
    } else if (CHECK_FILE_EXTENSION(filepath, ".woff2")) {
        type = "font/woff2";
    } else if (CHECK_FILE_EXTENSION(filepath, ".json")) {
        type = "application/json";
    }
    return httpd_resp_set_type(req, type);
}

// Set Cache-Control header based on file extension
static esp_err_t set_cache_control(httpd_req_t *req, const char *filepath)
{
    // default: ~30 days
    const char *cache = CACHE_POLICY_CACHE;

    // Fonts etc. can be cached "forever"
    if (CHECK_FILE_EXTENSION(filepath, ".woff2") ||
        CHECK_FILE_EXTENSION(filepath, ".png") ) {
        cache = CACHE_POLICY_IMMUTABLE;
    }

    // Set cache header
    return httpd_resp_set_hdr(req, "Cache-Control", cache);
}

static esp_err_t redirect_portal(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Redirecting to portal");
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t return_500(httpd_req_t *req, const char* msg)
{
    ESP_LOGE(TAG, "500 error: %s", msg);
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, msg);
    return ESP_FAIL;
}

/* Send HTTP response with the contents of the requested file */
esp_err_t rest_common_get_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    size_t filePathLength = sizeof(filepath);

    rest_server_context_t *rest_context = (rest_server_context_t *) req->user_ctx;

    // Initialize with base path
    strlcpy(filepath, rest_context->base_path, filePathLength);

    // Get requested URI
    size_t uri_len = strlen(req->uri);

    // Map "/foo/" -> "/foo/index.html"
    if (uri_len > 0 && req->uri[uri_len - 1] == '/') {
        if (strlen(filepath) + strlen("/index.html") + 1 > filePathLength) {
            return return_500(req, "File path too long");
        }
        strlcat(filepath, "/index.html", filePathLength);
    } else {
        // Check length before concatenating URI
        if (strlen(filepath) + uri_len + 1 > filePathLength) {
            return return_500(req, "File path too long");
        }
        strlcat(filepath, req->uri, filePathLength);
    }

    // Set Content-Type based on extension (.html, .js, ...)
    esp_err_t err = set_content_type_from_file(req, filepath);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "error setting content-type");
    }

    // Set Cache-Control header
    err = set_cache_control(req, filepath);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "error setting cache-control");
    }

    // Security header: tell browser not to guess MIME types
    httpd_resp_set_hdr(req, "X-Content-Type-Options", "nosniff");

    // Tell client we're going to close the socket after response.
    // This helps avoid piling up sockets on the ESP32.
    httpd_resp_set_hdr(req, "Connection", "close");

    // Now we append ".gz" because we serve precompressed assets.
    // Check buffer bounds first.
    if (strlen(filepath) + strlen(".gz") + 1 > filePathLength) {
        return return_500(req, "File path too long");
    }
    strlcat(filepath, ".gz", filePathLength);

    int fd = open(filepath, O_RDONLY, 0);
    if (fd < 0) {
        ESP_LOGE(TAG, "Failed to open file: %s, errno: %d", filepath, errno);
        if (errno == ENOENT) {
            // If asset not found, treat it like a captive portal / SPA redirect
            return redirect_portal(req);
        } else {
            return return_500(req, "Failed to open file");
        }
    }

    // At this point we KNOW we're actually serving a gzip file, so announce it.
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");

    ESP_LOGI(TAG, "Sending %s", filepath);

    // Ensure file is closed automatically when we leave scope
    FileGuard g(fd);

    char *chunk = rest_context->scratch;
    ssize_t read_bytes;
    for (;;) {
        // Read next chunk from SPIFFS
        read_bytes = read(fd, chunk, SCRATCH_BUFSIZE);
        if (read_bytes < 0) {
            return return_500(req, "Read error");
        }

        // EOF reached
        if (read_bytes == 0) {
            break;
        }

        // Send chunk to client. httpd_resp_send_chunk() will use chunked transfer encoding.
        if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to send file chunk");
            // Terminate chunked response explicitly before bailing out
            httpd_resp_send_chunk(req, NULL, 0);
            return ESP_FAIL;
        }
    }

    ESP_LOGI(TAG, "File sending complete");

    // Terminate chunked response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}
