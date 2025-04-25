
#include <fcntl.h>
#include <string.h>
#include <sys/param.h>
#include <netdb.h>

#include "ArduinoJson.h"
#include "psram_allocator.h"
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
#include "http_cors.h"
#include "http_utils.h"
#include "http_websocket.h"
#include "handler_influx.h"
#include "handler_swarm.h"

#include "history.h"
#include "boards/board.h"

#pragma GCC diagnostic error "-Wall"
#pragma GCC diagnostic error "-Wextra"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

#define max(a,b) ((a)>(b))?(a):(b)
#define min(a,b) ((a)<(b))?(a):(b)

static const char *TAG = "http_server";

httpd_handle_t http_server = NULL;

static void fillHistoryData(JsonObject &json_history, uint64_t start_timestamp, uint64_t end_timestamp, uint64_t current_timestamp);

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
    if (http_server) {
        // Stop the httpd server
        httpd_stop(http_server);
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

    // close connection to prevent clogging
    httpd_resp_set_hdr(req, "Connection", "close");

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
        Config::setStratumURL(doc["stratumURL"].as<const char*>());
    }
    if (doc["stratumUser"].is<const char*>()) {
        Config::setStratumUser(doc["stratumUser"].as<const char*>());
    }
    if (doc["stratumPassword"].is<const char*>()) {
        Config::setStratumPass(doc["stratumPassword"].as<const char*>());
    }
    if (doc["stratumPort"].is<uint16_t>()) {
        Config::setStratumPortNumber(doc["stratumPort"].as<uint16_t>());
    }
    if (doc["fallbackStratumURL"].is<const char*>()) {
        Config::setStratumFallbackURL(doc["fallbackStratumURL"].as<const char*>());
    }
    if (doc["fallbackStratumUser"].is<const char*>()) {
        Config::setStratumFallbackUser(doc["fallbackStratumUser"].as<const char*>());
    }
    if (doc["fallbackStratumPassword"].is<const char*>()) {
        Config::setStratumFallbackPass(doc["fallbackStratumPassword"].as<const char*>());
    }
    if (doc["fallbackStratumPort"].is<uint16_t>()) {
        Config::setStratumFallbackPortNumber(doc["fallbackStratumPort"].as<uint16_t>());
    }
    if (doc["ssid"].is<const char*>()) {
        Config::setWifiSSID(doc["ssid"].as<const char*>());
    }
    if (doc["wifiPass"].is<const char*>()) {
        Config::setWifiPass(doc["wifiPass"].as<const char*>());
    }
    if (doc["hostname"].is<const char*>()) {
        Config::setHostname(doc["hostname"].as<const char*>());
    }
    if (doc["coreVoltage"].is<uint16_t>()) {
        uint16_t coreVoltage = doc["coreVoltage"].as<uint16_t>();
        if (coreVoltage > 0) {
            Config::setAsicVoltage(coreVoltage);
        }
    }
    if (doc["frequency"].is<uint16_t>()) {
        uint16_t frequency = doc["frequency"].as<uint16_t>();
        if (frequency > 0) {
            Config::setAsicFrequency(frequency);
        }
    }
    if (doc["jobInterval"].is<uint16_t>()) {
        uint16_t jobInterval = doc["jobInterval"].as<uint16_t>();
        if (jobInterval > 0) {
            Config::setAsicJobInterval(jobInterval);
        }
    }
    if (doc["flipscreen"].is<bool>()) {
        Config::setFlipScreen(doc["flipscreen"].as<bool>());
    }
    if (doc["overheat_temp"].is<uint16_t>()) {
        Config::setOverheatTemp(doc["overheat_temp"].as<uint16_t>());
    }
    if (doc["invertscreen"].is<bool>()) {
        Config::setInvertScreen(doc["invertscreen"].as<bool>());
    }
    if (doc["invertfanpolarity"].is<bool>()) {
        Config::setInvertFanPolarity(doc["invertfanpolarity"].as<bool>());
    }
    if (doc["autofanpolarity"].is<bool>()) {
        Config::setAutoFanPolarity(doc["autofanpolarity"].as<bool>());
    }
    if (doc["autofanspeed"].is<uint16_t>()) {
        Config::setTempControlMode(doc["autofanspeed"].as<uint16_t>());
    }
    if (doc["fanspeed"].is<uint16_t>()) {
        Config::setFanSpeed(doc["fanspeed"].as<uint16_t>());
    }
    if (doc["autoscreenoff"].is<bool>()) {
        Config::setAutoScreenOff(doc["autoscreenoff"].as<bool>());
    }
    if (doc["pidTargetTemp"].is<uint16_t>()) {
        Config::setPidTargetTemp(doc["pidTargetTemp"].as<uint16_t>());
    }
    if (doc["pidP"].is<float>()) {
        Config::setPidP((uint16_t) (doc["pidP"].as<float>() * 100.0f));
    }
    if (doc["pidI"].is<float>()) {
        Config::setPidI((uint16_t) (doc["pidI"].as<float>() * 100.0f));
    }
    if (doc["pidD"].is<float>()) {
        Config::setPidD((uint16_t) (doc["pidD"].as<float>() * 100.0f));
    }

    doc.clear();

    // Signal the end of the response
    httpd_resp_send_chunk(req, NULL, 0);

    // Reload settings after update
    Board* board = SYSTEM_MODULE.getBoard();
    board->loadSettings();

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

    Board* board   = SYSTEM_MODULE.getBoard();
    History* history = SYSTEM_MODULE.getHistory();

    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);

    // Get configuration strings from NVS
    char *ssid               = Config::getWifiSSID();
    char *hostname           = Config::getHostname();
    char *stratumURL         = Config::getStratumURL();
    char *stratumUser        = Config::getStratumUser();
    char *fallbackStratumURL = Config::getStratumFallbackURL();
    char *fallbackStratumUser= Config::getStratumFallbackUser();

    // static
    doc["asicCount"]          = board->getAsicCount();
    doc["smallCoreCount"]     = (board->getAsics()) ? board->getAsics()->getSmallCoreCount() : 0;
    doc["deviceModel"]        = board->getDeviceModel();
    doc["hostip"]             = SYSTEM_MODULE.getIPAddress();
    doc["macAddr"]            = SYSTEM_MODULE.getMacAddress();

    // dashboard
    doc["power"]              = POWER_MANAGEMENT_MODULE.getPower();
    doc["maxPower"]           = board->getMaxPin();
    doc["minPower"]           = board->getMinPin();
    doc["voltage"]            = POWER_MANAGEMENT_MODULE.getVoltage();
    doc["maxVoltage"]         = board->getMaxVin();
    doc["minVoltage"]         = board->getMinVin();
    doc["current"]            = POWER_MANAGEMENT_MODULE.getCurrent();
    doc["temp"]               = POWER_MANAGEMENT_MODULE.getChipTempMax();
    doc["vrTemp"]             = POWER_MANAGEMENT_MODULE.getVRTemp();
    doc["hashRateTimestamp"]  = history->getCurrentTimestamp();
    doc["hashRate"]           = history->getCurrentHashrate10m();
    doc["hashRate_10m"]       = history->getCurrentHashrate10m();
    doc["hashRate_1h"]        = history->getCurrentHashrate1h();
    doc["hashRate_1d"]        = history->getCurrentHashrate1d();
    doc["bestDiff"]           = SYSTEM_MODULE.getBestDiffString();
    doc["bestSessionDiff"]    = SYSTEM_MODULE.getBestSessionDiffString();
    doc["coreVoltage"]        = board->getAsicVoltageMillis();
    doc["defaultCoreVoltage"] = board->getDefaultAsicVoltageMillis();
    doc["coreVoltageActual"]  = (int) (board->getVout() * 1000.0f);
    doc["sharesAccepted"]     = SYSTEM_MODULE.getSharesAccepted();
    doc["sharesRejected"]     = SYSTEM_MODULE.getSharesRejected();
    doc["isUsingFallbackStratum"] = STRATUM_MANAGER.isUsingFallback();
    doc["isStratumConnected"] = STRATUM_MANAGER.isAnyConnected();
    doc["fanspeed"]           = POWER_MANAGEMENT_MODULE.getFanPerc();
    doc["fanrpm"]             = POWER_MANAGEMENT_MODULE.getFanRPM();

    // If history was requested, add the history data as a nested object
    if (history_requested) {
        uint64_t end_timestamp = start_timestamp + 3600 * 1000ULL; // 1 hour later
        JsonObject json_history = doc["history"].to<JsonObject>();
        fillHistoryData(json_history, start_timestamp, end_timestamp, current_timestamp);
    }

    // settings
    PidSettings *pid = board->getPidSettings();
    doc["pidTargetTemp"]      = board->isPIDAvailable() ? pid->targetTemp : -1;
    doc["pidP"]               = (float) pid->p / 100.0f;
    doc["pidI"]               = (float) pid->i / 100.0f;
    doc["pidD"]               = (float) pid->d / 100.0f;

    doc["hostname"]           = hostname;
    doc["ssid"]               = ssid;
    doc["stratumURL"]         = stratumURL;
    doc["stratumPort"]        = Config::getStratumPortNumber();
    doc["stratumUser"]        = stratumUser;
    doc["fallbackStratumURL"] = fallbackStratumURL;
    doc["fallbackStratumPort"]= Config::getStratumFallbackPortNumber();
    doc["fallbackStratumUser"] = fallbackStratumUser;
    doc["voltage"]            = POWER_MANAGEMENT_MODULE.getVoltage();
    doc["frequency"]          = board->getAsicFrequency();
    doc["defaultFrequency"]   = board->getDefaultAsicFrequency();
    doc["jobInterval"]        = board->getAsicJobIntervalMs();
    doc["overheat_temp"]      = Config::getOverheatTemp();
    doc["flipscreen"]         = board->isFlipScreenEnabled() ? 1 : 0;
    doc["invertscreen"]       = Config::isInvertScreenEnabled() ? 1 : 0; // unused?
    doc["autoscreenoff"]      = Config::isAutoScreenOffEnabled() ? 1 : 0;
    doc["invertfanpolarity"]  = board->isInvertFanPolarityEnabled() ? 1 : 0;
    doc["autofanpolarity"]  = board->isAutoFanPolarityEnabled() ? 1 : 0;
    doc["autofanspeed"]       = Config::getTempControlMode();

    // system screen
    doc["ASICModel"]          = board->getAsicModel();
    doc["uptimeSeconds"]      = (esp_timer_get_time() - SYSTEM_MODULE.getStartTime()) / 1000000;
    doc["lastResetReason"]    = SYSTEM_MODULE.getLastResetReason();
    doc["wifiStatus"]         = SYSTEM_MODULE.getWifiStatus();
    doc["freeHeap"]           = esp_get_free_heap_size();
    doc["version"]            = esp_app_get_description()->version;
    doc["runningPartition"]   = esp_ota_get_running_partition()->label;

    //ESP_LOGI(TAG, "allocs: %d, deallocs: %d, reallocs: %d", allocs, deallocs, reallocs);

    // close connection to prevent clogging
    httpd_resp_set_hdr(req, "Connection", "close");

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

    rest_server_context_t *rest_context = (rest_server_context_t*) CALLOC(1, sizeof(rest_server_context_t));
    if (!rest_context) {
        ESP_LOGE(TAG, "No memory for rest context");
        return ESP_FAIL;
    }

    strlcpy(rest_context->base_path, base_path, sizeof(rest_context->base_path));



    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 20;
    config.lru_purge_enable = true;
    config.max_open_sockets = 10;
    config.stack_size = 12288;

    ESP_LOGI(TAG, "Starting HTTP Server");
    if (httpd_start(&http_server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Start server failed");
        free(rest_context);
        return ESP_FAIL;
    }

    httpd_uri_t recovery_explicit_get_uri = {
        .uri = "/recovery", .method = HTTP_GET, .handler = rest_recovery_handler, .user_ctx = rest_context};
    httpd_register_uri_handler(http_server, &recovery_explicit_get_uri);

    /* URI handler for fetching system info */
    httpd_uri_t system_info_get_uri = {
        .uri = "/api/system/info", .method = HTTP_GET, .handler = GET_system_info, .user_ctx = rest_context};
    httpd_register_uri_handler(http_server, &system_info_get_uri);

    /* URI handler for fetching system info */
    httpd_uri_t influx_info_get_uri = {
        .uri = "/api/influx/info", .method = HTTP_GET, .handler = GET_influx_info, .user_ctx = rest_context};
    httpd_register_uri_handler(http_server, &influx_info_get_uri);

    httpd_uri_t swarm_get_uri = {.uri = "/api/swarm/info", .method = HTTP_GET, .handler = GET_swarm, .user_ctx = rest_context};
    httpd_register_uri_handler(http_server, &swarm_get_uri);

    httpd_uri_t update_swarm_uri = {
        .uri = "/api/swarm", .method = HTTP_PATCH, .handler = PATCH_update_swarm, .user_ctx = rest_context};
    httpd_register_uri_handler(http_server, &update_swarm_uri);

    httpd_uri_t swarm_options_uri = {
        .uri = "/api/swarm",
        .method = HTTP_OPTIONS,
        .handler = handle_options_request,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(http_server, &swarm_options_uri);

    httpd_uri_t system_restart_uri = {
        .uri = "/api/system/restart", .method = HTTP_POST, .handler = POST_restart, .user_ctx = rest_context};
    httpd_register_uri_handler(http_server, &system_restart_uri);

    httpd_uri_t update_system_settings_uri = {
        .uri = "/api/system", .method = HTTP_PATCH, .handler = PATCH_update_settings, .user_ctx = rest_context};
    httpd_register_uri_handler(http_server, &update_system_settings_uri);

    httpd_uri_t update_influx_settings_uri = {
        .uri = "/api/influx", .method = HTTP_PATCH, .handler = PATCH_update_influx, .user_ctx = rest_context};
    httpd_register_uri_handler(http_server, &update_influx_settings_uri);

    httpd_uri_t system_options_uri = {
        .uri = "/api/system",
        .method = HTTP_OPTIONS,
        .handler = handle_options_request,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(http_server, &system_options_uri);

    httpd_uri_t update_post_ota_firmware = {
        .uri = "/api/system/OTA", .method = HTTP_POST, .handler = POST_OTA_update, .user_ctx = NULL};
    httpd_register_uri_handler(http_server, &update_post_ota_firmware);

    httpd_uri_t update_post_ota_www = {
        .uri = "/api/system/OTAWWW", .method = HTTP_POST, .handler = POST_WWW_update, .user_ctx = NULL};
    httpd_register_uri_handler(http_server, &update_post_ota_www);

    httpd_uri_t ws = {.uri = "/api/ws", .method = HTTP_GET, .handler = echo_handler, .user_ctx = NULL, .is_websocket = true};
    httpd_register_uri_handler(http_server, &ws);

    if (enter_recovery) {
        /* Make default route serve Recovery */
        httpd_uri_t recovery_implicit_get_uri = {
            .uri = "/*", .method = HTTP_GET, .handler = rest_recovery_handler, .user_ctx = rest_context};
        httpd_register_uri_handler(http_server, &recovery_implicit_get_uri);

    } else {
        /* URI handler for getting web http_server files */
        httpd_uri_t common_get_uri = {
            .uri = "/*", .method = HTTP_GET, .handler = rest_common_get_handler, .user_ctx = rest_context};
        httpd_register_uri_handler(http_server, &common_get_uri);
    }

    httpd_register_err_handler(http_server, HTTPD_404_NOT_FOUND, http_404_error_handler);

    websocket_start();

    // Start the DNS server that will redirect all queries to the softAP IP
    dns_server_config_t dns_config = DNS_SERVER_CONFIG_SINGLE("*" /* all A queries */, "WIFI_AP_DEF" /* softAP netif ID */);
    start_dns_server(&dns_config);

    return ESP_OK;
}
