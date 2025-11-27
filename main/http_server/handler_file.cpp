#include <fcntl.h>
#include <string.h>

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"

#include "http_server.h"
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

    // don't cache the index.html
    if (CHECK_FILE_EXTENSION(filepath, ".html")) {
        cache = CACHE_POLICY_NO_CACHE;
    }

    // Fonts etc. can be cached "forever"
    if (CHECK_FILE_EXTENSION(filepath, ".woff2") ||
        CHECK_FILE_EXTENSION(filepath, ".png") ) {
        cache = CACHE_POLICY_IMMUTABLE;
    }

    // Set cache header
    return httpd_resp_set_hdr(req, "Cache-Control", cache);
}

static bool is_asset_request(const char *uri)
{
    const char *ext = strrchr(uri, '.');
    if (!ext) {
        // no extension - we assume it's a page
        return false;
    }

    return (
        strcmp(ext, ".js")  == 0 ||
        strcmp(ext, ".css") == 0 ||
        strcmp(ext, ".png") == 0 ||
        strcmp(ext, ".jpg") == 0 ||
        strcmp(ext, ".jpeg") == 0 ||
        strcmp(ext, ".svg") == 0 ||
        strcmp(ext, ".ico") == 0 ||
        strcmp(ext, ".woff2") == 0 ||
        strcmp(ext, ".json") == 0
    );
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
    // close connection when out of scope
    ConGuard g(http_server, req);

    char filepath[FILE_PATH_MAX];
    size_t filePathLength = sizeof(filepath);

    rest_server_context_t *rest_context = (rest_server_context_t *) req->user_ctx;

    // Initialize with base path
    strlcpy(filepath, rest_context->base_path, filePathLength);

    // Strip query string from URI (e.g. "?v=abc123") ---
    char uri_clean[FILE_PATH_MAX];
    strlcpy(uri_clean, req->uri, sizeof(uri_clean));
    char *qmark = strchr(uri_clean, '?');
    if (qmark) {
        *qmark = '\0';  // terminate before '?'
    }

    size_t uri_len = strlen(uri_clean);

    // Map "/foo/" -> "/foo/index.html"
    if (uri_len > 0 && uri_clean[uri_len - 1] == '/') {
        if (strlen(filepath) + strlen("/index.html") + 1 > filePathLength) {
            // Ensure "Connection: close" on errors
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "File path too long");
        }
        strlcat(filepath, "/index.html", filePathLength);
    } else {
        // Check length before concatenating URI
        if (strlen(filepath) + uri_len + 1 > filePathLength) {
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "File path too long");
        }
        strlcat(filepath, uri_clean, filePathLength);
    }

    // Set core headers before sending any body
    // MIME type by extension (.html, .js, .css, .json, ...)
    (void)set_content_type_from_file(req, filepath);
    // Cache policy by extension
    (void)set_cache_control(req, filepath);

    // Security header: do not MIME-sniff
    httpd_resp_set_hdr(req, "X-Content-Type-Options", "nosniff");

    // We serve precompressed assets -> append ".gz"
    if (strlen(filepath) + strlen(".gz") + 1 > filePathLength) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "File path too long");
    }
    strlcat(filepath, ".gz", filePathLength);

    // Try open file
    int fd = open(filepath, O_RDONLY, 0);
    if (fd < 0) {
        ESP_LOGE(TAG, "Failed to open file: %s, errno: %d", filepath, errno);
        if (errno == ENOENT) {
            // asset vs page
            if (is_asset_request(uri_clean)) {
                // asset missing -> 404 and no portal redirection
                httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not found");
                return ESP_OK;
            } else {
                // portal redirection
                return redirect_portal(req);
            }
        } else {
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to open file");
        }
    }

    FileGuard fg(fd); // ensure file closed on exit

    // Announce gzip content encoding
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");

    ESP_LOGI(TAG, "Sending %s", filepath);

    // Stream file using chunked transfer and finish with a zero-length chunk
    char *chunk = rest_context->scratch;
    ssize_t read_bytes;

    for (;;) {
        read_bytes = read(fd, chunk, SCRATCH_BUFSIZE);
        if (read_bytes < 0) {
            ESP_LOGE(TAG, "Read error while sending %s", filepath);
            // Terminate chunked response explicitly before reporting failure
            httpd_resp_send_chunk(req, NULL, 0);
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Read error");
        }

        if (read_bytes == 0) {
            // EOF
            break;
        }

        if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to send file chunk");
            // Try to terminate chunked response; ignore result
            httpd_resp_send_chunk(req, NULL, 0);
            return ESP_FAIL;
        }
    }

    ESP_LOGI(TAG, "File sending complete");

    // Final zero-length chunk to end chunked body
    httpd_resp_send_chunk(req, NULL, 0);

    return ESP_OK;
}
