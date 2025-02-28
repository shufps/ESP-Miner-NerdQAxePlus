
#include <fcntl.h>
#include <string.h>
#include <sys/param.h>
#include <netdb.h>

#include <ArduinoJson.h>
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

static void fillHistoryData(JsonObject &json_history, uint64_t start_timestamp, uint64_t end_timestamp, uint64_t current_timestamp);

static int allocs = 0;
static int deallocs = 0;
static int reallocs = 0;

struct PSRAMAllocator : ArduinoJson::Allocator {
  void* allocate(size_t size) override {
    allocs++;
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
  }

  void deallocate(void* pointer) override {
    deallocs++;
    heap_caps_free(pointer);
  }

  void* reallocate(void* ptr, size_t new_size) override {
    reallocs++;
    return heap_caps_realloc(ptr, new_size, MALLOC_CAP_SPIRAM);
  }
};

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

static uint32_t get_origin_ip(const char *host)
{
    uint32_t origin_ip_addr = 0;

    // Convert the IP address string to uint32_t
    origin_ip_addr = inet_addr(host);
    if (origin_ip_addr == INADDR_NONE) {
        ESP_LOGW(CORS_TAG, "Invalid IP address: %s", host);
    } else {
        ESP_LOGI(CORS_TAG, "Extracted IP address %lu", origin_ip_addr);
    }
    return origin_ip_addr;
}


static const char* extract_origin_host(char *origin)
{
    const char *prefix = "http://";

    origin = strstr(origin, prefix);
    if (!origin) {
        return nullptr;
    }

    // skip prefix
    char *host = &origin[strlen(prefix)];

    // find the slash
    char *prefix_end = strchr(host, '/');

    // if we have one, we just terminate the string
    if (prefix_end) {
        *prefix_end = 0;
    }

    // in the other case we keep the previous zero termination
    return (const char*) host;
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
        const char *host = extract_origin_host(origin);
        if (!host) {
            ESP_LOGW(CORS_TAG, "couldn't extract origin host: %s", origin);
            return ESP_FAIL;
        }
        ESP_LOGI(CORS_TAG, "extracted origin host: %s", host);

        // check if origin is hostname
        const char *hostname = SYSTEM_MODULE.getHostname();
        ESP_LOGI(CORS_TAG, "hostname: %s", hostname);
        if (!(strncmp(host, hostname, strlen(hostname)))) {
            ESP_LOGI(CORS_TAG, "origin equals hostname");
            return ESP_OK;
        }

        origin_ip_addr = get_origin_ip(host);
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

    if (strstr(req->uri, ".woff2")) {
        httpd_resp_set_hdr(req, "Content-Type", "font/woff2");
        httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=31536000, immutable");
    } else if (req->uri[strlen(req->uri) - 1] != '/') {
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

static esp_err_t sendJsonResponse(httpd_req_t *req, JsonDocument &doc) {
    // Measure the size needed for the JSON text
    size_t jsonLength = measureJson(doc);
    // Allocate a buffer from PSRAM (or regular heap if you prefer)
    char *jsonOutput = (char*) heap_caps_malloc(jsonLength + 1, MALLOC_CAP_SPIRAM);
    if (!jsonOutput) {
        ESP_LOGE(TAG, "Failed to allocate memory for JSON output");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation error");
        return ESP_FAIL;
    }
    // Serialize the JSON document into the allocated buffer
    serializeJson(doc, jsonOutput, jsonLength + 1);
    // Send the response
    esp_err_t ret = httpd_resp_sendstr(req, jsonOutput);
    // Free the allocated buffer
    heap_caps_free(jsonOutput);
    return ret;
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

    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);

    // Parse the JSON payload
    DeserializationError error = deserializeJson(doc, buf);
    if (error) {
        ESP_LOGE(TAG, "JSON parsing failed: %s", error.c_str());
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    // Update settings if each key exists in the JSON object.
    if (doc["stratumURL"].is<const char*>()) {
        nvs_config_set_string(NVS_CONFIG_STRATUM_URL, doc["stratumURL"].as<const char*>());
    }
    if (doc["stratumUser"].is<const char*>()) {
        nvs_config_set_string(NVS_CONFIG_STRATUM_USER, doc["stratumUser"].as<const char*>());
    }
    if (doc["stratumPassword"].is<const char*>()) {
        nvs_config_set_string(NVS_CONFIG_STRATUM_PASS, doc["stratumPassword"].as<const char*>());
    }
    if (doc["stratumPort"].is<uint16_t>()) {
        nvs_config_set_u16(NVS_CONFIG_STRATUM_PORT, doc["stratumPort"].as<uint16_t>());
    }
    if (doc["fallbackStratumURL"].is<const char*>()) {
        nvs_config_set_string(NVS_CONFIG_STRATUM_FALLBACK_URL, doc["fallbackStratumURL"].as<const char*>());
    }
    if (doc["fallbackStratumUser"].is<const char*>()) {
        nvs_config_set_string(NVS_CONFIG_STRATUM_FALLBACK_USER, doc["fallbackStratumUser"].as<const char*>());
    }
    if (doc["fallbackStratumPassword"].is<const char*>()) {
        nvs_config_set_string(NVS_CONFIG_STRATUM_FALLBACK_PASS, doc["fallbackStratumPassword"].as<const char*>());
    }
    if (doc["fallbackStratumPort"].is<uint16_t>()) {
        nvs_config_set_u16(NVS_CONFIG_STRATUM_FALLBACK_PORT, doc["fallbackStratumPort"].as<uint16_t>());
    }
    if (doc["ssid"].is<const char*>()) {
        nvs_config_set_string(NVS_CONFIG_WIFI_SSID, doc["ssid"].as<const char*>());
    }
    if (doc["wifiPass"].is<const char*>()) {
        nvs_config_set_string(NVS_CONFIG_WIFI_PASS, doc["wifiPass"].as<const char*>());
    }
    if (doc["hostname"].is<const char*>()) {
        nvs_config_set_string(NVS_CONFIG_HOSTNAME, doc["hostname"].as<const char*>());
    }
    if (doc["coreVoltage"].is<uint16_t>()) {
        uint16_t coreVoltage = doc["coreVoltage"].as<uint16_t>();
        if (coreVoltage > 0) {
            nvs_config_set_u16(NVS_CONFIG_ASIC_VOLTAGE, coreVoltage);
        }
    }
    if (doc["frequency"].is<uint16_t>()) {
        uint16_t frequency = doc["frequency"].as<uint16_t>();
        if (frequency > 0) {
            nvs_config_set_u16(NVS_CONFIG_ASIC_FREQ, frequency);
        }
    }
    if (doc["jobInterval"].is<uint16_t>()) {
        uint16_t jobInterval = doc["jobInterval"].as<uint16_t>();
        if (jobInterval > 0) {
            nvs_config_set_u16(NVS_CONFIG_ASIC_JOB_INTERVAL, jobInterval);
        }
    }
    if (doc["flipscreen"].is<bool>()) {
        nvs_config_set_u16(NVS_CONFIG_FLIP_SCREEN, (uint16_t) doc["flipscreen"].as<bool>());
    }
    if (doc["overheat_temp"].is<uint16_t>()) {
        nvs_config_set_u16(NVS_CONFIG_OVERHEAT_TEMP, doc["overheat_temp"].as<uint16_t>());
    }
    if (doc["invertscreen"].is<bool>()) {
        nvs_config_set_u16(NVS_CONFIG_INVERT_SCREEN, (uint16_t) doc["invertscreen"].as<bool>());
    }
    if (doc["invertfanpolarity"].is<bool>()) {
        nvs_config_set_u16(NVS_CONFIG_INVERT_FAN_POLARITY, (uint16_t) doc["invertfanpolarity"].as<bool>());
    }
    if (doc["autofanspeed"].is<bool>()) {
        nvs_config_set_u16(NVS_CONFIG_AUTO_FAN_SPEED, (uint16_t) doc["autofanspeed"].as<bool>());
    }
    if (doc["fanspeed"].is<uint16_t>()) {
        nvs_config_set_u16(NVS_CONFIG_FAN_SPEED, doc["fanspeed"].as<uint16_t>());
    }
    if (doc["autoscreenoff"].is<bool>()) {
        nvs_config_set_u16(NVS_CONFIG_AUTO_SCREEN_OFF, (uint16_t) doc["autoscreenoff"].as<bool>());
    }

    doc.clear();

    // Signal the end of the response
    httpd_resp_send_chunk(req, NULL, 0);

    // Reload settings after update
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
    char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
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

    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);

    // Parse the JSON payload
    DeserializationError error = deserializeJson(doc, buf);
    if (error) {
        ESP_LOGE(TAG, "JSON parsing failed: %s", error.c_str());
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    // Check and apply each setting if the key exists and has the correct type
    if (doc["influxEnable"].is<bool>()) {
        nvs_config_set_u16(NVS_CONFIG_INFLUX_ENABLE, (uint16_t) doc["influxEnable"].as<bool>());
    }
    if (doc["influxURL"].is<const char*>()) {
        nvs_config_set_string(NVS_CONFIG_INFLUX_URL, doc["influxURL"].as<const char*>());
    }
    if (doc["influxPort"].is<uint16_t>()) {
        nvs_config_set_u16(NVS_CONFIG_INFLUX_PORT, doc["influxPort"].as<uint16_t>());
    }
    if (doc["influxToken"].is<const char*>()) {
        nvs_config_set_string(NVS_CONFIG_INFLUX_TOKEN, doc["influxToken"].as<const char*>());
    }
    if (doc["influxBucket"].is<const char*>()) {
        nvs_config_set_string(NVS_CONFIG_INFLUX_BUCKET, doc["influxBucket"].as<const char*>());
    }
    if (doc["influxOrg"].is<const char*>()) {
        nvs_config_set_string(NVS_CONFIG_INFLUX_ORG, doc["influxOrg"].as<const char*>());
    }
    if (doc["influxPrefix"].is<const char*>()) {
        nvs_config_set_string(NVS_CONFIG_INFLUX_PREFIX, doc["influxPrefix"].as<const char*>());
    }

    doc.clear();

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

    Board* board   = SYSTEM_MODULE.getBoard();
    History* history = SYSTEM_MODULE.getHistory();

    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);

    // Get configuration strings from NVS
    char *ssid               = nvs_config_get_string(NVS_CONFIG_WIFI_SSID, CONFIG_ESP_WIFI_SSID);
    char *hostname           = nvs_config_get_string(NVS_CONFIG_HOSTNAME, CONFIG_LWIP_LOCAL_HOSTNAME);
    char *stratumURL         = nvs_config_get_string(NVS_CONFIG_STRATUM_URL, CONFIG_STRATUM_URL);
    char *stratumUser        = nvs_config_get_string(NVS_CONFIG_STRATUM_USER, CONFIG_STRATUM_USER);
    char *fallbackStratumURL = nvs_config_get_string(NVS_CONFIG_STRATUM_FALLBACK_URL, CONFIG_STRATUM_FALLBACK_URL);
    char *fallbackStratumUser= nvs_config_get_string(NVS_CONFIG_STRATUM_FALLBACK_USER, CONFIG_STRATUM_FALLBACK_USER);

    // static
    doc["asicCount"]          = board->getAsicCount();
    doc["smallCoreCount"]     = (board->getAsics()) ? board->getAsics()->getSmallCoreCount() : 0;
    doc["deviceModel"]        = board->getDeviceModel();
    doc["hostip"]             = SYSTEM_MODULE.getIPAddress();

    // dashboard
    doc["power"]              = POWER_MANAGEMENT_MODULE.getPower();
    doc["maxPower"]           = board->getParams()->maxPin;
    doc["minPower"]           = board->getParams()->minPin;
    doc["voltage"]            = POWER_MANAGEMENT_MODULE.getVoltage();
    doc["maxVoltage"]         = board->getParams()->maxVin;
    doc["minVoltage"]         = board->getParams()->minVin;
    doc["current"]            = POWER_MANAGEMENT_MODULE.getCurrent();
    doc["temp"]               = POWER_MANAGEMENT_MODULE.getAvgChipTemp();
    doc["vrTemp"]             = POWER_MANAGEMENT_MODULE.getVrTemp();
    doc["hashRateTimestamp"]  = history->getCurrentTimestamp();
    doc["hashRate"]           = history->getCurrentHashrate10m();
    doc["hashRate_10m"]       = history->getCurrentHashrate10m();
    doc["hashRate_1h"]        = history->getCurrentHashrate1h();
    doc["hashRate_1d"]        = history->getCurrentHashrate1d();
    doc["bestDiff"]           = SYSTEM_MODULE.getBestDiffString();
    doc["bestSessionDiff"]    = SYSTEM_MODULE.getBestSessionDiffString();
    doc["coreVoltage"]        = nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE, CONFIG_ASIC_VOLTAGE);
    doc["coreVoltageActual"]  = (int)(board->getVout() * 1000.0f);
    doc["sharesAccepted"]     = SYSTEM_MODULE.getSharesAccepted();
    doc["sharesRejected"]     = SYSTEM_MODULE.getSharesRejected();
    doc["isUsingFallbackStratum"] = STRATUM_MANAGER.isUsingFallback();
    doc["fanspeed"]           = POWER_MANAGEMENT_MODULE.getFanPerc();
    doc["fanrpm"]             = POWER_MANAGEMENT_MODULE.getFanRPM();

    // If history was requested, add the history data as a nested object
    if (history_requested) {
        uint64_t end_timestamp = start_timestamp + 3600 * 1000ULL; // 1 hour later
        JsonObject json_history = doc["history"].to<JsonObject>();
        fillHistoryData(json_history, start_timestamp, end_timestamp, current_timestamp);
    }

    doc["hostname"]           = hostname;
    doc["ssid"]               = ssid;
    doc["stratumURL"]         = stratumURL;
    doc["stratumPort"]        = nvs_config_get_u16(NVS_CONFIG_STRATUM_PORT, CONFIG_STRATUM_PORT);
    doc["stratumUser"]        = stratumUser;
    doc["fallbackStratumURL"] = fallbackStratumURL;
    doc["fallbackStratumPort"]= nvs_config_get_u16(NVS_CONFIG_STRATUM_FALLBACK_PORT, CONFIG_STRATUM_FALLBACK_PORT);
    doc["fallbackStratumUser"] = fallbackStratumUser;
    doc["voltage"]            = POWER_MANAGEMENT_MODULE.getVoltage();
    doc["frequency"]          = nvs_config_get_u16(NVS_CONFIG_ASIC_FREQ, CONFIG_ASIC_FREQUENCY);
    doc["jobInterval"]        = board->getAsicJobIntervalMs();
    doc["overheat_temp"]      = nvs_config_get_u16(NVS_CONFIG_OVERHEAT_TEMP, CONFIG_OVERHEAT_TEMP);
    doc["flipscreen"]         = nvs_config_get_u16(NVS_CONFIG_FLIP_SCREEN, CONFIG_FLIP_SCREEN_VALUE);
    doc["invertscreen"]       = nvs_config_get_u16(NVS_CONFIG_INVERT_SCREEN, 0); // unused?
    doc["autoscreenoff"]      = nvs_config_get_u16(NVS_CONFIG_AUTO_SCREEN_OFF, CONFIG_AUTO_SCREEN_OFF_VALUE);
    doc["invertfanpolarity"]  = nvs_config_get_u16(NVS_CONFIG_INVERT_FAN_POLARITY, CONFIG_INVERT_POLARITY_VALUE);
    doc["autofanspeed"]       = nvs_config_get_u16(NVS_CONFIG_AUTO_FAN_SPEED, CONFIG_AUTO_FAN_SPEED_VALUE);

    // system screen
    doc["ASICModel"]          = board->getAsicModel();
    doc["uptimeSeconds"]      = (esp_timer_get_time() - SYSTEM_MODULE.getStartTime()) / 1000000;
    doc["lastResetReason"]    = SYSTEM_MODULE.getLastResetReason();
    doc["wifiStatus"]         = SYSTEM_MODULE.getWifiStatus();
    doc["freeHeap"]           = esp_get_free_heap_size();
    doc["version"]            = esp_app_get_description()->version;
    doc["runningPartition"]   = esp_ota_get_running_partition()->label;

    ESP_LOGI(TAG, "allocs: %d, deallocs: %d, reallocs: %d", allocs, deallocs, reallocs);

    // Serialize the JSON document to a String and send it
    esp_err_t ret = sendJsonResponse(req, doc);
    doc.clear();

    // Free temporary strings
    free(ssid);
    free(hostname);
    free(stratumURL);
    free(stratumUser);
    free(fallbackStratumURL);
    free(fallbackStratumUser);

    return ret;
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

    // Retrieve configuration strings from NVS
    char *influxURL    = nvs_config_get_string(NVS_CONFIG_INFLUX_URL, CONFIG_INFLUX_URL);
    char *influxBucket = nvs_config_get_string(NVS_CONFIG_INFLUX_BUCKET, CONFIG_INFLUX_BUCKET);
    char *influxOrg    = nvs_config_get_string(NVS_CONFIG_INFLUX_ORG, CONFIG_INFLUX_ORG);
    char *influxPrefix = nvs_config_get_string(NVS_CONFIG_INFLUX_PREFIX, CONFIG_INFLUX_PREFIX);

    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);

    // Fill the JSON object with values
    doc["influxURL"]    = influxURL;
    doc["influxPort"]   = nvs_config_get_u16(NVS_CONFIG_INFLUX_PORT, CONFIG_INFLUX_PORT);
    doc["influxBucket"] = influxBucket;
    doc["influxOrg"]    = influxOrg;
    doc["influxPrefix"] = influxPrefix;
    doc["influxEnable"] = nvs_config_get_u16(NVS_CONFIG_INFLUX_ENABLE, CONFIG_INFLUX_ENABLE_VALUE);

    // Serialize the JSON document into a string (using Arduino's String type)
    esp_err_t ret = sendJsonResponse(req, doc);

    doc.clear();

    // Free temporary strings from NVS retrieval
    free(influxURL);
    free(influxBucket);
    free(influxOrg);
    free(influxPrefix);

    return ret;
}

// Helper: fills a JsonObject with history data using ArduinoJson
static void fillHistoryData(JsonObject &json_history, uint64_t start_timestamp, uint64_t end_timestamp, uint64_t current_timestamp) {
    History *history = SYSTEM_MODULE.getHistory();

    // Ensure consistency
    history->lock();

    int64_t rel_start = (int64_t) start_timestamp - (int64_t) current_timestamp;
    int64_t rel_end   = (int64_t) end_timestamp - (int64_t) current_timestamp;

    // Get current system timestamp (in ms)
    uint64_t sys_timestamp = esp_timer_get_time() / 1000ULL;
    int64_t sys_start = (int64_t) sys_timestamp + rel_start;
    int64_t sys_end   = (int64_t) sys_timestamp + rel_end;

    int start_index = history->searchNearestTimestamp(sys_start);
    int end_index   = history->searchNearestTimestamp(sys_end);
    int num_samples = end_index - start_index + 1;

    if (!history->isAvailable() || start_index == -1 || end_index == -1 ||
        num_samples <= 0 || (end_index < start_index)) {
        ESP_LOGW(TAG, "Invalid history indices or history not (yet) available");
        // If the data is invalid, return an empty object
        num_samples = 0;
    }

    // Create arrays for history samples using the new method
    JsonArray hashrate_10m = json_history["hashrate_10m"].to<JsonArray>();
    JsonArray hashrate_1h  = json_history["hashrate_1h"].to<JsonArray>();
    JsonArray hashrate_1d  = json_history["hashrate_1d"].to<JsonArray>();
    JsonArray timestamps   = json_history["timestamps"].to<JsonArray>();

    for (int i = start_index; i < start_index + num_samples; i++) {
        uint64_t sample_timestamp = history->getTimestampSample(i);
        if ((int64_t) sample_timestamp < sys_start) {
            continue;
        }
        // Multiply by 100.0 and cast to int as in the original code
        hashrate_10m.add((int)(history->getHashrate10mSample(i) * 100.0));
        hashrate_1h.add((int)(history->getHashrate1hSample(i) * 100.0));
        hashrate_1d.add((int)(history->getHashrate1dSample(i) * 100.0));
        timestamps.add((int64_t) sample_timestamp - sys_start);
    }

    // Add base timestamp for reference
    json_history["timestampBase"] = start_timestamp;

    history->unlock();
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
    config.stack_size = 8192;

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
