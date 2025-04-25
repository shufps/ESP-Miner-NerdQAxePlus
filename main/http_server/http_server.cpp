
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
#include "handler_system.h"
#include "handler_ota.h"
#include "handler_restart.h"
#include "handler_file.h"

#include "history.h"
#include "boards/board.h"

#pragma GCC diagnostic error "-Wall"
#pragma GCC diagnostic error "-Wextra"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"



static const char *TAG = "http_server";

httpd_handle_t http_server = NULL;

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


/* Recovery handler */
static esp_err_t rest_recovery_handler(httpd_req_t *req)
{
    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }
    httpd_resp_send(req, recovery_page, HTTPD_RESP_USE_STRLEN);
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
