
#include <fcntl.h>
#include <string.h>
#include <sys/param.h>
#include <netdb.h>

#include "cJSON.h"
#include "esp_chip_info.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_spiffs.h"
#include "esp_timer.h"
#include "esp_vfs.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_ota_ops.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "dns_server.h"
#include "lwip/err.h"
#include "lwip/inet.h"
#include "lwip/lwip_napt.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"

#include "global_state.h"
#include "nvs_config.h"
#include "recovery_page.h"
#include "http_server.h"

#include "history.h"
#include "boards/board.h"

#pragma GCC diagnostic error "-Wall"
#pragma GCC diagnostic error "-Wextra"
#pragma GCC diagnostic error "-Wmissing-prototypes"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

#define max(a,b) ((a)>(b))?(a):(b)
#define min(a,b) ((a)<(b))?(a):(b)

static const char *TAG = "http_server";
static const char * CORS_TAG = "CORS";

static httpd_handle_t server = NULL;
QueueHandle_t log_queue = NULL;

static int fd = -1;

#define REST_CHECK(a, str, goto_tag, ...)                                                                                          \
    do {                                                                                                                           \
        if (!(a)) {                                                                                                                \
            ESP_LOGE(TAG, "%s(%d): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__);                                                  \
            goto goto_tag;                                                                                                         \
        }                                                                                                                          \
    } while (0)

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + 128)
#define SCRATCH_BUFSIZE (10240)
#define MESSAGE_QUEUE_SIZE (128)

typedef struct rest_server_context
{
    char base_path[ESP_VFS_PATH_MAX + 1];
    char scratch[SCRATCH_BUFSIZE];
} rest_server_context_t;

#define CHECK_FILE_EXTENSION(filename, ext) (strcasecmp(&filename[strlen(filename) - strlen(ext)], ext) == 0)

static cJSON *get_history_data(uint64_t start_timestamp, uint64_t end_timestamp, uint64_t current_timestamp);

static void *psram_malloc(size_t size)
{
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
}

static void psram_free(void *ptr)
{
    heap_caps_free(ptr);
}

static void configure_cjson_for_psram()
{
    cJSON_Hooks hooks;
    hooks.malloc_fn = psram_malloc;
    hooks.free_fn = psram_free;
    cJSON_InitHooks(&hooks);
}

static esp_err_t ip_in_private_range(uint32_t address) {
    uint32_t ip_address = ntohl(address);

    // 10.0.0.0 - 10.255.255.255 (Class A)
    if ((ip_address >= 0x0A000000) && (ip_address <= 0x0AFFFFFF)) {
        return ESP_OK;
    }

    // 172.16.0.0 - 172.31.255.255 (Class B)
    if ((ip_address >= 0xAC100000) && (ip_address <= 0xAC1FFFFF)) {
        return ESP_OK;
    }

    // 192.168.0.0 - 192.168.255.255 (Class C)
    if ((ip_address >= 0xC0A80000) && (ip_address <= 0xC0A8FFFF)) {
        return ESP_OK;
    }

    return ESP_FAIL;
}

static uint32_t resolve_origin_ip_addr(char *origin)
{
    char host[64];     // Buffer for the extracted host (IP or hostname)
    uint32_t origin_ip_addr = 0;
    struct hostent *he;

    // Find the start of the hostname/IP in the Origin header
    const char *prefix = "http://";
    char *host_start = strstr(origin, prefix);
    if (host_start) {
        host_start += strlen(prefix); // Move past "http://"

        // Extract the host part (everything before the first '/')
        char *host_end = strchr(host_start, '/');
        size_t host_len = host_end ? (size_t)(host_end - host_start) : strlen(host_start);

        // Ensure extracted string fits within host buffer
        if (host_len >= sizeof(host)) {
            ESP_LOGW(CORS_TAG, "Hostname is too long: %s", host_start);
            return 0;
        }

        // Copy hostname/IP into `host` buffer and null-terminate it
        strncpy(host, host_start, host_len);
        host[host_len] = '\0';

        // Directly resolve using gethostbyname()
        he = gethostbyname(host);
        if (he && he->h_addr_list[0]) {
            origin_ip_addr = *(uint32_t *)he->h_addr_list[0];
            ESP_LOGI(CORS_TAG, "Resolved %s to IP %s", host, inet_ntoa(*(struct in_addr*)&origin_ip_addr));
        } else {
            ESP_LOGW(CORS_TAG, "Failed to resolve hostname: %s", host);
            origin_ip_addr = 0;
        }
    }

    return origin_ip_addr;
}

static esp_err_t is_network_allowed(httpd_req_t * req)
{
    if (SYSTEM_MODULE.getAPState()) {
        ESP_LOGI(CORS_TAG, "Device in AP mode. Allowing CORS.");
        return ESP_OK;
    }

    int sockfd = httpd_req_to_sockfd(req);
    char ipstr[INET6_ADDRSTRLEN];
    struct sockaddr_in6 addr;   // esp_http_server uses IPv6 addressing
    socklen_t addr_size = sizeof(addr);

    if (getpeername(sockfd, (struct sockaddr *)&addr, &addr_size) < 0) {
        ESP_LOGE(CORS_TAG, "Error getting client IP");
        return ESP_FAIL;
    }

    uint32_t request_ip_addr = addr.sin6_addr.un.u32_addr[3];

    // // Convert to IPv6 string
    // inet_ntop(AF_INET, &addr.sin6_addr, ipstr, sizeof(ipstr));

    // Convert to IPv4 string
    inet_ntop(AF_INET, &request_ip_addr, ipstr, sizeof(ipstr));

    // Attempt to get the Origin header.
    char origin[128];
    uint32_t origin_ip_addr;
    if (httpd_req_get_hdr_value_str(req, "Origin", origin, sizeof(origin)) == ESP_OK) {
        ESP_LOGD(CORS_TAG, "Origin header: %s", origin);
        origin_ip_addr = resolve_origin_ip_addr(origin);
    } else {
        ESP_LOGD(CORS_TAG, "No origin header found.");
        origin_ip_addr = request_ip_addr;
    }

    if (ip_in_private_range(origin_ip_addr) == ESP_OK && ip_in_private_range(request_ip_addr) == ESP_OK) {
        return ESP_OK;
    }

    ESP_LOGI(CORS_TAG, "Client is NOT in the private ip ranges or same range as server.");
    return ESP_FAIL;
}

static esp_err_t init_fs(void)
{
    esp_vfs_spiffs_conf_t conf = {.base_path = "", .partition_label = NULL, .max_files = 5, .format_if_mount_failed = false};
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

/* Function for stopping the webserver */
/*
static void stop_webserver(httpd_handle_t server)
{
    if (server) {
        // Stop the httpd server
        httpd_stop(server);
    }
}
*/

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
        type = "text/xml";
    }
    return httpd_resp_set_type(req, type);
}
static esp_err_t set_cors_headers(httpd_req_t *req)
{

    return httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*") == ESP_OK &&
                   httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, PUT, PATCH, DELETE, OPTIONS") == ESP_OK &&
                   httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type") == ESP_OK
               ? ESP_OK
               : ESP_FAIL;
}

/* Recovery handler */
static esp_err_t rest_recovery_handler(httpd_req_t *req)
{
    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }
    httpd_resp_send(req, recovery_page, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* Send HTTP response with the contents of the requested file */
static esp_err_t rest_common_get_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    size_t filePathLength = sizeof(filepath);  // Fixed size type

    rest_server_context_t *rest_context = (rest_server_context_t *) req->user_ctx;
    strlcpy(filepath, rest_context->base_path, filePathLength);

    if (req->uri[strlen(req->uri) - 1] == '/') {
        strlcat(filepath, "/index.html", filePathLength);
    } else {
        if (strlen(filepath) + strlen(req->uri) + 1 > filePathLength) {
            ESP_LOGE(TAG, "File path too long!");
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "File path too long");
            return ESP_FAIL;
        }
        strlcat(filepath, req->uri, filePathLength);
    }

    set_content_type_from_file(req, filepath);
    strlcat(filepath, ".gz", filePathLength);  // Append .gz extension

    int fd = open(filepath, O_RDONLY, 0);
    if (fd == -1) {
        ESP_LOGE(TAG, "Failed to open file: %s, errno: %d", filepath, errno);

        if (errno == ENOENT) {
            httpd_resp_set_status(req, "302 Temporary Redirect");
            httpd_resp_set_hdr(req, "Location", "/");
            httpd_resp_send(req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN);
            ESP_LOGI(TAG, "Redirecting to root");
            return ESP_OK;
        } else {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to open file");
            return ESP_FAIL;
        }
    }

    if (req->uri[strlen(req->uri) - 1] != '/') {
        httpd_resp_set_hdr(req, "Cache-Control", "max-age=2592000");
    }

    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");

    char *chunk = rest_context->scratch;
    ssize_t read_bytes;
    do {
        read_bytes = read(fd, chunk, SCRATCH_BUFSIZE);
        if (read_bytes == -1) {
            ESP_LOGE(TAG, "Failed to read file: %s", filepath);
            close(fd);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Read error");
            return ESP_FAIL;
        } else if (read_bytes > 0) {
            if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK) {
                ESP_LOGE(TAG, "File sending failed!");
                close(fd);
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
                return ESP_FAIL;
            }
        }
    } while (read_bytes > 0);

    close(fd);
    ESP_LOGI(TAG, "File sending complete");
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}


static esp_err_t PATCH_update_swarm(httpd_req_t *req)
{
    // Set CORS headers
    if (set_cors_headers(req) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    int total_len = req->content_len;
    int cur_len = 0;
    char *buf = ((rest_server_context_t *) (req->user_ctx))->scratch;
    int received = 0;
    if (total_len >= SCRATCH_BUFSIZE) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }
    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len);
        if (received <= 0) {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';

    nvs_config_set_string(NVS_CONFIG_SWARM, buf);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t handle_options_request(httpd_req_t *req)
{
    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    // Set CORS headers for OPTIONS request
    if (set_cors_headers(req) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Send a blank response for OPTIONS request
    httpd_resp_send(req, NULL, 0);

    return ESP_OK;
}

static esp_err_t PATCH_update_settings(httpd_req_t *req)
{
    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    // Set CORS headers
    if (set_cors_headers(req) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    int total_len = req->content_len;
    int cur_len = 0;
    char *buf = ((rest_server_context_t *) (req->user_ctx))->scratch;
    int received = 0;
    if (total_len >= SCRATCH_BUFSIZE) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }
    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len);
        if (received <= 0) {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    cJSON *item;
    if ((item = cJSON_GetObjectItem(root, "stratumURL")) != NULL) {
        nvs_config_set_string(NVS_CONFIG_STRATUM_URL, item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(root, "stratumUser")) != NULL) {
        nvs_config_set_string(NVS_CONFIG_STRATUM_USER, item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(root, "stratumPassword")) != NULL) {
        nvs_config_set_string(NVS_CONFIG_STRATUM_PASS, item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(root, "stratumPort")) != NULL) {
        nvs_config_set_u16(NVS_CONFIG_STRATUM_PORT, item->valueint);
    }
    if ((item = cJSON_GetObjectItem(root, "fallbackStratumURL")) != NULL) {
        nvs_config_set_string(NVS_CONFIG_STRATUM_FALLBACK_URL, item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(root, "fallbackStratumUser")) != NULL) {
        nvs_config_set_string(NVS_CONFIG_STRATUM_FALLBACK_USER, item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(root, "fallbackStratumPassword")) != NULL) {
        nvs_config_set_string(NVS_CONFIG_STRATUM_FALLBACK_PASS, item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(root, "fallbackStratumPort")) != NULL) {
        nvs_config_set_u16(NVS_CONFIG_STRATUM_FALLBACK_PORT, item->valueint);
    }
    if ((item = cJSON_GetObjectItem(root, "ssid")) != NULL) {
        nvs_config_set_string(NVS_CONFIG_WIFI_SSID, item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(root, "wifiPass")) != NULL) {
        nvs_config_set_string(NVS_CONFIG_WIFI_PASS, item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(root, "hostname")) != NULL) {
        nvs_config_set_string(NVS_CONFIG_HOSTNAME, item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(root, "coreVoltage")) != NULL && item->valueint > 0) {
        nvs_config_set_u16(NVS_CONFIG_ASIC_VOLTAGE, item->valueint);
    }
    if ((item = cJSON_GetObjectItem(root, "frequency")) != NULL && item->valueint > 0) {
        nvs_config_set_u16(NVS_CONFIG_ASIC_FREQ, item->valueint);
    }
    if ((item = cJSON_GetObjectItem(root, "jobInterval")) != NULL && item->valueint > 0) {
        nvs_config_set_u16(NVS_CONFIG_ASIC_JOB_INTERVAL, item->valueint);
    }
    if ((item = cJSON_GetObjectItem(root, "flipscreen")) != NULL) {
        nvs_config_set_u16(NVS_CONFIG_FLIP_SCREEN, item->valueint);
    }
    if ((item = cJSON_GetObjectItem(root, "overheat_temp")) != NULL) {
        nvs_config_set_u16(NVS_CONFIG_OVERHEAT_TEMP, item->valueint);
    }
    if ((item = cJSON_GetObjectItem(root, "invertscreen")) != NULL) {
        nvs_config_set_u16(NVS_CONFIG_INVERT_SCREEN, item->valueint);
    }
    if ((item = cJSON_GetObjectItem(root, "invertfanpolarity")) != NULL) {
        nvs_config_set_u16(NVS_CONFIG_INVERT_FAN_POLARITY, item->valueint);
    }
    if ((item = cJSON_GetObjectItem(root, "autofanspeed")) != NULL) {
        nvs_config_set_u16(NVS_CONFIG_AUTO_FAN_SPEED, item->valueint);
    }
    if ((item = cJSON_GetObjectItem(root, "fanspeed")) != NULL) {
        nvs_config_set_u16(NVS_CONFIG_FAN_SPEED, item->valueint);
    }
    if ((item = cJSON_GetObjectItem(root, "autoscreenoff")) != NULL) {
        nvs_config_set_u16(NVS_CONFIG_AUTO_SCREEN_OFF, item->valueint);
    }

    cJSON_Delete(root);
    httpd_resp_send_chunk(req, NULL, 0);

    // reload settings
    Board* board = SYSTEM_MODULE.getBoard();
    board->loadSettings();

    return ESP_OK;
}

static esp_err_t PATCH_update_influx(httpd_req_t *req)
{
    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    // Set CORS headers
    if (set_cors_headers(req) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    int total_len = req->content_len;
    int cur_len = 0;
    char *buf = ((rest_server_context_t *) (req->user_ctx))->scratch;
    int received = 0;
    if (total_len >= SCRATCH_BUFSIZE) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }
    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len);
        if (received <= 0) {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    cJSON *item;
    if ((item = cJSON_GetObjectItem(root, "influxEnable")) != NULL) {
        nvs_config_set_u16(NVS_CONFIG_INFLUX_ENABLE, item->valueint);
    }
    if ((item = cJSON_GetObjectItem(root, "influxURL")) != NULL) {
        nvs_config_set_string(NVS_CONFIG_INFLUX_URL, item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(root, "influxPort")) != NULL) {
        nvs_config_set_u16(NVS_CONFIG_INFLUX_PORT, item->valueint);
    }
    if ((item = cJSON_GetObjectItem(root, "influxToken")) != NULL) {
        nvs_config_set_string(NVS_CONFIG_INFLUX_TOKEN, item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(root, "influxBucket")) != NULL) {
        nvs_config_set_string(NVS_CONFIG_INFLUX_BUCKET, item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(root, "influxOrg")) != NULL) {
        nvs_config_set_string(NVS_CONFIG_INFLUX_ORG, item->valuestring);
    }
    if ((item = cJSON_GetObjectItem(root, "influxPrefix")) != NULL) {
        nvs_config_set_string(NVS_CONFIG_INFLUX_PREFIX, item->valuestring);
    }

    cJSON_Delete(root);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t POST_restart(httpd_req_t *req)
{
    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    ESP_LOGI(TAG, "Restarting System because of API Request");

    // Send HTTP response before restarting
    const char *resp_str = "System will restart shortly.";
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);

    // Delay to ensure the response is sent
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    // Restart the system
    POWER_MANAGEMENT_MODULE.restart();

    // This return statement will never be reached, but it's good practice to include it
    return ESP_OK;
}

static esp_err_t GET_swarm(httpd_req_t *req)
{
    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    httpd_resp_set_type(req, "application/json");

    // Set CORS headers
    if (set_cors_headers(req) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    char *swarm_config = nvs_config_get_string(NVS_CONFIG_SWARM, "[]");
    httpd_resp_sendstr(req, swarm_config);
    free(swarm_config);
    return ESP_OK;
}

/* Simple handler for getting system handler */
static esp_err_t GET_system_info(httpd_req_t *req)
{
    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    httpd_resp_set_type(req, "application/json");

    // Set CORS headers
    if (set_cors_headers(req) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Parse optional start_timestamp parameter
    uint64_t start_timestamp = 0;
    uint64_t current_timestamp = 0;
    bool history_requested = false;
    char query_str[128];
    if (httpd_req_get_url_query_str(req, query_str, sizeof(query_str)) == ESP_OK) {
        char param[64];
        if (httpd_query_key_value(query_str, "ts", param, sizeof(param)) == ESP_OK) {
            start_timestamp = strtoull(param, NULL, 10);
            if (start_timestamp) {
                history_requested = true;
            }
        }
        if (httpd_query_key_value(query_str, "cur", param, sizeof(param)) == ESP_OK) {
            current_timestamp = strtoull(param, NULL, 10);
            ESP_LOGI(TAG, "cur: %llu", current_timestamp);
        }
    }

    // Gather system info as before
    char *ssid = nvs_config_get_string(NVS_CONFIG_WIFI_SSID, CONFIG_ESP_WIFI_SSID);
    char *hostname = nvs_config_get_string(NVS_CONFIG_HOSTNAME, CONFIG_LWIP_LOCAL_HOSTNAME);

    char *stratumURL = nvs_config_get_string(NVS_CONFIG_STRATUM_URL, CONFIG_STRATUM_URL);
    char *stratumUser = nvs_config_get_string(NVS_CONFIG_STRATUM_USER, CONFIG_STRATUM_USER);

    char *fallbackStratumURL = nvs_config_get_string(NVS_CONFIG_STRATUM_FALLBACK_URL, CONFIG_STRATUM_FALLBACK_URL);
    char *fallbackStratumUser = nvs_config_get_string(NVS_CONFIG_STRATUM_FALLBACK_USER, CONFIG_STRATUM_FALLBACK_USER);

    Board* board = SYSTEM_MODULE.getBoard();
    History* history = SYSTEM_MODULE.getHistory();

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "power", POWER_MANAGEMENT_MODULE.getPower());
    cJSON_AddNumberToObject(root, "maxPower", board->getMaxPin());
    cJSON_AddNumberToObject(root, "minPower", board->getMinPin());
    cJSON_AddNumberToObject(root, "voltage", POWER_MANAGEMENT_MODULE.getVoltage());
    cJSON_AddNumberToObject(root, "maxVoltage", board->getMaxVin());
    cJSON_AddNumberToObject(root, "minVoltage", board->getMinVin());
    cJSON_AddNumberToObject(root, "current", POWER_MANAGEMENT_MODULE.getCurrent());
    cJSON_AddNumberToObject(root, "temp", POWER_MANAGEMENT_MODULE.getAvgChipTemp());
    cJSON_AddNumberToObject(root, "vrTemp", POWER_MANAGEMENT_MODULE.getVRTemp());
    cJSON_AddNumberToObject(root, "hashRateTimestamp", history->getCurrentTimestamp());
    cJSON_AddNumberToObject(root, "hashRate", history->getCurrentHashrate10m());
    cJSON_AddNumberToObject(root, "hashRate_10m", history->getCurrentHashrate10m());
    cJSON_AddNumberToObject(root, "hashRate_1h", history->getCurrentHashrate1h());
    cJSON_AddNumberToObject(root, "hashRate_1d", history->getCurrentHashrate1d());
    cJSON_AddNumberToObject(root, "jobInterval", board->getAsicJobIntervalMs());
    cJSON_AddStringToObject(root, "bestDiff", SYSTEM_MODULE.getBestDiffString());
    cJSON_AddStringToObject(root, "bestSessionDiff", SYSTEM_MODULE.getBestSessionDiffString());

    cJSON_AddNumberToObject(root, "freeHeap", esp_get_free_heap_size());
    cJSON_AddNumberToObject(root, "coreVoltage", nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE, CONFIG_ASIC_VOLTAGE));
    cJSON_AddNumberToObject(root, "coreVoltageActual", (int) (board->getVout() * 1000.0f));
    cJSON_AddNumberToObject(root, "frequency", nvs_config_get_u16(NVS_CONFIG_ASIC_FREQ, CONFIG_ASIC_FREQUENCY));
    cJSON_AddStringToObject(root, "ssid", ssid);
    cJSON_AddStringToObject(root, "hostname", hostname);
    cJSON_AddStringToObject(root, "wifiStatus", SYSTEM_MODULE.getWifiStatus());
    cJSON_AddNumberToObject(root, "sharesAccepted", SYSTEM_MODULE.getSharesAccepted());
    cJSON_AddNumberToObject(root, "sharesRejected", SYSTEM_MODULE.getSharesRejected());
    cJSON_AddNumberToObject(root, "uptimeSeconds", (esp_timer_get_time() - SYSTEM_MODULE.getStartTime()) / 1000000);
    cJSON_AddNumberToObject(root, "asicCount", board->getAsicCount());
    cJSON_AddNumberToObject(root, "smallCoreCount", (board->getAsics()) ? board->getAsics()->getSmallCoreCount() : 0);
    cJSON_AddStringToObject(root, "ASICModel", board->getAsicModel());
    cJSON_AddStringToObject(root, "deviceModel", board->getDeviceModel());
    cJSON_AddStringToObject(root, "stratumURL", stratumURL);
    cJSON_AddNumberToObject(root, "stratumPort", nvs_config_get_u16(NVS_CONFIG_STRATUM_PORT, CONFIG_STRATUM_PORT));
    cJSON_AddStringToObject(root, "stratumUser", stratumUser);
    cJSON_AddStringToObject(root, "fallbackStratumURL", fallbackStratumURL);
    cJSON_AddNumberToObject(root, "fallbackStratumPort", nvs_config_get_u16(NVS_CONFIG_STRATUM_FALLBACK_PORT, CONFIG_STRATUM_FALLBACK_PORT));
    cJSON_AddStringToObject(root, "fallbackStratumUser", fallbackStratumUser);
    cJSON_AddNumberToObject(root, "isUsingFallbackStratum", STRATUM_MANAGER.isUsingFallback());
    cJSON_AddStringToObject(root, "version", esp_ota_get_app_description()->version);
    cJSON_AddStringToObject(root, "runningPartition", esp_ota_get_running_partition()->label);
    cJSON_AddNumberToObject(root, "flipscreen", nvs_config_get_u16(NVS_CONFIG_FLIP_SCREEN, 1));
    cJSON_AddNumberToObject(root, "overheat_temp", nvs_config_get_u16(NVS_CONFIG_OVERHEAT_TEMP, 0));
    cJSON_AddNumberToObject(root, "invertscreen", nvs_config_get_u16(NVS_CONFIG_INVERT_SCREEN, 0));
    cJSON_AddNumberToObject(root, "autoscreenoff", nvs_config_get_u16(NVS_CONFIG_AUTO_SCREEN_OFF, 0));
    cJSON_AddNumberToObject(root, "invertfanpolarity", nvs_config_get_u16(NVS_CONFIG_INVERT_FAN_POLARITY, 1));
    cJSON_AddNumberToObject(root, "autofanspeed", nvs_config_get_u16(NVS_CONFIG_AUTO_FAN_SPEED, 1));
    cJSON_AddNumberToObject(root, "fanspeed", POWER_MANAGEMENT_MODULE.getFanPerc());
    cJSON_AddNumberToObject(root, "fanrpm", POWER_MANAGEMENT_MODULE.getFanRPM());
    cJSON_AddStringToObject(root, "lastResetReason", SYSTEM_MODULE.getLastResetReason());
    // If start_timestamp is provided, include history data
    if (history_requested) {
        uint64_t end_timestamp = start_timestamp + 3600 * 1000ULL; // 1 hour after start_timestamp
        cJSON *history = get_history_data(start_timestamp, end_timestamp, current_timestamp);
        cJSON_AddItemToObject(root, "history", history);
    }

    free(ssid);
    free(hostname);
    free(stratumURL);
    free(stratumUser);
    free(fallbackStratumURL);
    free(fallbackStratumUser);

    const char *sys_info = cJSON_PrintUnformatted(root);
    httpd_resp_sendstr(req, sys_info);
    free((void*) sys_info);
    cJSON_Delete(root);
    return ESP_OK;
}


/* Simple handler for getting system handler */
static esp_err_t GET_influx_info(httpd_req_t *req)
{
    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    httpd_resp_set_type(req, "application/json");

    // Set CORS headers
    if (set_cors_headers(req) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    char *influxURL = nvs_config_get_string(NVS_CONFIG_INFLUX_URL, CONFIG_INFLUX_URL);
    char *influxBucket = nvs_config_get_string(NVS_CONFIG_INFLUX_BUCKET, CONFIG_INFLUX_BUCKET);
    char *influxOrg = nvs_config_get_string(NVS_CONFIG_INFLUX_ORG, CONFIG_INFLUX_ORG);
    char *influxPrefix = nvs_config_get_string(NVS_CONFIG_INFLUX_PREFIX, CONFIG_INFLUX_PREFIX);

    cJSON *root = cJSON_CreateObject();

    cJSON_AddStringToObject(root, "influxURL", influxURL);
    cJSON_AddNumberToObject(root, "influxPort", nvs_config_get_u16(NVS_CONFIG_INFLUX_PORT, CONFIG_INFLUX_PORT));
    cJSON_AddStringToObject(root, "influxBucket", influxBucket);
    cJSON_AddStringToObject(root, "influxOrg", influxOrg);
    cJSON_AddStringToObject(root, "influxPrefix", influxPrefix);
    cJSON_AddNumberToObject(root, "influxEnable", nvs_config_get_u16(NVS_CONFIG_INFLUX_ENABLE, 1));

    free(influxURL);
    free(influxBucket);
    free(influxOrg);
    free(influxPrefix);

    const char *influx_info = cJSON_PrintUnformatted(root);
    httpd_resp_sendstr(req, influx_info);
    free((char*) influx_info);
    cJSON_Delete(root);
    return ESP_OK;
}

static cJSON *get_history_data(uint64_t start_timestamp, uint64_t end_timestamp, uint64_t current_timestamp)
{
    History *history = SYSTEM_MODULE.getHistory();

    // Ensure consistency
    history->lock();

    int64_t rel_start = (int64_t) start_timestamp - (int64_t) current_timestamp;
    int64_t rel_end = (int64_t) end_timestamp - (int64_t) current_timestamp;

    // get current system timestamp since system boot in ms
    uint64_t sys_timestamp = esp_timer_get_time() / 1000llu;
    int64_t sys_start = (int64_t) sys_timestamp + rel_start;
    int64_t sys_end = (int64_t) sys_timestamp + rel_end;

    int start_index = history->searchNearestTimestamp(sys_start);
    int end_index = history->searchNearestTimestamp(sys_end);
    int num_samples = end_index - start_index + 1;

    cJSON *json_history = cJSON_CreateObject();

    if (!history->isAvailable() || start_index == -1 || end_index == -1 || num_samples <= 0 || (end_index < start_index)) {
        ESP_LOGW(TAG, "Invalid history indices or history not (yet) available");
        // If the data is invalid, return an empty object
        num_samples = 0;
    }

    cJSON *json_hashrate_10m = cJSON_CreateArray();
    cJSON *json_hashrate_1h = cJSON_CreateArray();
    cJSON *json_hashrate_1d = cJSON_CreateArray();
    cJSON *json_timestamps = cJSON_CreateArray();

    for (int i = start_index; i < start_index + num_samples; i++) {
        uint64_t sample_timestamp = history->getTimestampSample(i);

        if ((int64_t) sample_timestamp < sys_start) {
            continue;
        }

        cJSON_AddItemToArray(json_hashrate_10m, cJSON_CreateNumber((int)(history->getHashrate10mSample(i) * 100.0)));
        cJSON_AddItemToArray(json_hashrate_1h, cJSON_CreateNumber((int)(history->getHashrate1hSample(i) * 100.0)));
        cJSON_AddItemToArray(json_hashrate_1d, cJSON_CreateNumber((int)(history->getHashrate1dSample(i) * 100.0)));
        cJSON_AddItemToArray(json_timestamps, cJSON_CreateNumber((int64_t) sample_timestamp - sys_start));
    }

    cJSON_AddItemToObject(json_history, "hashrate_10m", json_hashrate_10m);
    cJSON_AddItemToObject(json_history, "hashrate_1h", json_hashrate_1h);
    cJSON_AddItemToObject(json_history, "hashrate_1d", json_hashrate_1d);
    cJSON_AddItemToObject(json_history, "timestamps", json_timestamps);
    cJSON_AddNumberToObject(json_history, "timestampBase", start_timestamp);

    history->unlock();

    return json_history;
}

static esp_err_t POST_WWW_update(httpd_req_t *req)
{
    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    int remaining = req->content_len;

    const esp_partition_t *www_partition =
        esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, "www");
    if (www_partition == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "WWW partition not found");
        return ESP_FAIL;
    }

    // Don't attempt to write more than what can be stored in the partition
    if (remaining > www_partition->size) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File provided is too large for device");
        return ESP_FAIL;
    }

    // Erase the entire www partition before writing
    ESP_ERROR_CHECK(esp_partition_erase_range(www_partition, 0, www_partition->size));

    // don't put it on the stack
    char *buf = (char*) malloc(2048);

    while (remaining > 0) {
        int recv_len = httpd_req_recv(req, buf, MIN(remaining, 2048));

        if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;
        } else if (recv_len <= 0) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Protocol Error");
            free(buf);
            return ESP_FAIL;
        }

        if (esp_partition_write(www_partition, www_partition->size - remaining, (const void *) buf, recv_len) != ESP_OK) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Write Error");
            free(buf);
            return ESP_FAIL;
        }

        remaining -= recv_len;
    }

    free(buf);

    httpd_resp_sendstr(req, "WWW update complete\n");
    return ESP_OK;
}

/*
 * Handle OTA file upload
 */
static esp_err_t POST_OTA_update(httpd_req_t *req)
{
    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    esp_ota_handle_t ota_handle;
    int remaining = req->content_len;

    const esp_partition_t *ota_partition = esp_ota_get_next_update_partition(NULL);
    ESP_ERROR_CHECK(esp_ota_begin(ota_partition, OTA_SIZE_UNKNOWN, &ota_handle));

    // don't put it on the stack
    char *buf = (char*) malloc(2048);

    while (remaining > 0) {
        int recv_len = httpd_req_recv(req, buf, MIN(remaining, 2048));

        // Timeout Error: Just retry
        if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;

            // Serious Error: Abort OTA
        } else if (recv_len <= 0) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Protocol Error");
            free(buf);
            return ESP_FAIL;
        }

        // Successful Upload: Flash firmware chunk
        if (esp_ota_write(ota_handle, (const void *) buf, recv_len) != ESP_OK) {
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Flash Error");
            free(buf);
            return ESP_FAIL;
        }

        remaining -= recv_len;
    }

    free(buf);

    // Validate and switch to new OTA image and reboot
    if (esp_ota_end(ota_handle) != ESP_OK || esp_ota_set_boot_partition(ota_partition) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Validation / Activation Error");
        return ESP_FAIL;
    }

    httpd_resp_sendstr(req, "Firmware update complete, rebooting now!\n");
    ESP_LOGI(TAG, "Restarting System because of Firmware update complete");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    POWER_MANAGEMENT_MODULE.restart();

    return ESP_OK;
}

static int log_to_queue(const char * format, va_list args)
{
    va_list args_copy;
    va_copy(args_copy, args);

    // Calculate the required buffer size
    int needed_size = vsnprintf(NULL, 0, format, args_copy) + 1;
    va_end(args_copy);

    // Allocate the buffer dynamically
    char * log_buffer = (char *) calloc(needed_size + 2, sizeof(char));  // +2 for potential \n and \0
    if (log_buffer == NULL) {
        return 0;
    }

    // Format the string into the allocated buffer
    va_copy(args_copy, args);
    vsnprintf(log_buffer, needed_size, format, args_copy);
    va_end(args_copy);

    // Ensure the log message ends with a newline
    size_t len = strlen(log_buffer);
    if (len > 0 && log_buffer[len - 1] != '\n') {
        log_buffer[len] = '\n';
        log_buffer[len + 1] = '\0';
        len++;
    }

    // Print to standard output
    printf("%s", log_buffer);

	if (xQueueSendToBack(log_queue, (void*)&log_buffer, (TickType_t) 0) != pdPASS) {
		if (log_buffer != NULL) {
			free((void*)log_buffer);
		}
	}
    return 0;
}

static void send_log_to_websocket(char *message)
{
    // Prepare the WebSocket frame
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t *)message;
    ws_pkt.len = strlen(message);
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    // Ensure server and fd are valid
    if (server != NULL && fd >= 0) {
        // Send the WebSocket frame asynchronously
        if (httpd_ws_send_frame_async(server, fd, &ws_pkt) != ESP_OK) {
            esp_log_set_vprintf(vprintf);
        }
    }

    // Free the allocated buffer
    free((void*)message);
}

/*
 * This handler echos back the received ws data
 * and triggers an async send if certain message received
 */
static esp_err_t echo_handler(httpd_req_t *req)
{
    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "Handshake done, the new connection was opened");
        fd = httpd_req_to_sockfd(req);
        esp_log_set_vprintf(log_to_queue);
        return ESP_OK;
    }
    return ESP_OK;
}

// HTTP Error (404) Handler - Redirects all requests to the root page
static esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    // Set status
    httpd_resp_set_status(req, "302 Temporary Redirect");
    // Redirect to the "/" root directory
    httpd_resp_set_hdr(req, "Location", "/");
    // iOS requires content in the response to detect a captive portal, simply redirecting is not sufficient.
    httpd_resp_send(req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN);

    ESP_LOGI(TAG, "Redirecting to root");
    return ESP_OK;
}

static void websocket_log_handler(void* param)
{
	while (true)
	{
		char *message;
		if (xQueueReceive(log_queue, &message, (TickType_t) portMAX_DELAY) != pdPASS) {
			if (message != NULL) {
				free((void*)message);
			}
			vTaskDelay(10 / portTICK_PERIOD_MS);
			continue;
		}

		if (fd == -1) {
			free((void*)message);
			vTaskDelay(100 / portTICK_PERIOD_MS);
			continue;
		}

		send_log_to_websocket(message);
	}
}

esp_err_t start_rest_server(void * pvParameters)
{
    configure_cjson_for_psram();

    const char *base_path = "";

    bool enter_recovery = false;
    if (init_fs() != ESP_OK) {
        // Unable to initialize the web app filesystem.
        // Enter recovery mode
        enter_recovery = true;
    }

    if (!base_path) {
        ESP_LOGE(TAG, "wrong base path");
        return ESP_FAIL;
    }

    rest_server_context_t *rest_context = (rest_server_context_t*) calloc(1, sizeof(rest_server_context_t));
    if (!rest_context) {
        ESP_LOGE(TAG, "No memory for rest context");
        return ESP_FAIL;
    }

    strlcpy(rest_context->base_path, base_path, sizeof(rest_context->base_path));

    log_queue = xQueueCreate(MESSAGE_QUEUE_SIZE, sizeof(char*));

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 20;
    config.lru_purge_enable = true;
    config.max_open_sockets = 10;

    ESP_LOGI(TAG, "Starting HTTP Server");
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Start server failed");
        free(rest_context);
        return ESP_FAIL;
    }

    httpd_uri_t recovery_explicit_get_uri = {
        .uri = "/recovery", .method = HTTP_GET, .handler = rest_recovery_handler, .user_ctx = rest_context};
    httpd_register_uri_handler(server, &recovery_explicit_get_uri);

    /* URI handler for fetching system info */
    httpd_uri_t system_info_get_uri = {
        .uri = "/api/system/info", .method = HTTP_GET, .handler = GET_system_info, .user_ctx = rest_context};
    httpd_register_uri_handler(server, &system_info_get_uri);

    /* URI handler for fetching system info */
    httpd_uri_t influx_info_get_uri = {
        .uri = "/api/influx/info", .method = HTTP_GET, .handler = GET_influx_info, .user_ctx = rest_context};
    httpd_register_uri_handler(server, &influx_info_get_uri);

    httpd_uri_t swarm_get_uri = {.uri = "/api/swarm/info", .method = HTTP_GET, .handler = GET_swarm, .user_ctx = rest_context};
    httpd_register_uri_handler(server, &swarm_get_uri);

    httpd_uri_t update_swarm_uri = {
        .uri = "/api/swarm", .method = HTTP_PATCH, .handler = PATCH_update_swarm, .user_ctx = rest_context};
    httpd_register_uri_handler(server, &update_swarm_uri);

    httpd_uri_t swarm_options_uri = {
        .uri = "/api/swarm",
        .method = HTTP_OPTIONS,
        .handler = handle_options_request,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(server, &swarm_options_uri);

    httpd_uri_t system_restart_uri = {
        .uri = "/api/system/restart", .method = HTTP_POST, .handler = POST_restart, .user_ctx = rest_context};
    httpd_register_uri_handler(server, &system_restart_uri);

    httpd_uri_t update_system_settings_uri = {
        .uri = "/api/system", .method = HTTP_PATCH, .handler = PATCH_update_settings, .user_ctx = rest_context};
    httpd_register_uri_handler(server, &update_system_settings_uri);

    httpd_uri_t update_influx_settings_uri = {
        .uri = "/api/influx", .method = HTTP_PATCH, .handler = PATCH_update_influx, .user_ctx = rest_context};
    httpd_register_uri_handler(server, &update_influx_settings_uri);

    httpd_uri_t system_options_uri = {
        .uri = "/api/system",
        .method = HTTP_OPTIONS,
        .handler = handle_options_request,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(server, &system_options_uri);

    httpd_uri_t update_post_ota_firmware = {
        .uri = "/api/system/OTA", .method = HTTP_POST, .handler = POST_OTA_update, .user_ctx = NULL};
    httpd_register_uri_handler(server, &update_post_ota_firmware);

    httpd_uri_t update_post_ota_www = {
        .uri = "/api/system/OTAWWW", .method = HTTP_POST, .handler = POST_WWW_update, .user_ctx = NULL};
    httpd_register_uri_handler(server, &update_post_ota_www);

    httpd_uri_t ws = {.uri = "/api/ws", .method = HTTP_GET, .handler = echo_handler, .user_ctx = NULL, .is_websocket = true};
    httpd_register_uri_handler(server, &ws);

    if (enter_recovery) {
        /* Make default route serve Recovery */
        httpd_uri_t recovery_implicit_get_uri = {
            .uri = "/*", .method = HTTP_GET, .handler = rest_recovery_handler, .user_ctx = rest_context};
        httpd_register_uri_handler(server, &recovery_implicit_get_uri);

    } else {
        /* URI handler for getting web server files */
        httpd_uri_t common_get_uri = {
            .uri = "/*", .method = HTTP_GET, .handler = rest_common_get_handler, .user_ctx = rest_context};
        httpd_register_uri_handler(server, &common_get_uri);
    }

    httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler);

    // Start websocket log handler thread
    xTaskCreate(&websocket_log_handler, "websocket_log_handler", 4096, NULL, 2, NULL);

    // Start the DNS server that will redirect all queries to the softAP IP
    dns_server_config_t dns_config = DNS_SERVER_CONFIG_SINGLE("*" /* all A queries */, "WIFI_AP_DEF" /* softAP netif ID */);
    start_dns_server(&dns_config);

    return ESP_OK;
}
