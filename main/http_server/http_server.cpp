
#include "esp_http_server.h"
#include "esp_https_server.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "dns_server.h"

#include "global_state.h"
#include "handler_file.h"
#include "handler_influx.h"
#include "handler_ota.h"
#include "handler_restart.h"
#include "handler_swarm.h"
#include "handler_system.h"
#include "http_cors.h"
#include "http_server.h"
#include "http_utils.h"
#include "http_websocket.h"
#include "nvs_config.h"
#include "recovery_page.h"

#pragma GCC diagnostic error "-Wall"
#pragma GCC diagnostic error "-Wextra"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

static const char *TAG = "http_server";

extern const uint8_t server_crt_start[] asm("_binary_server_crt_start");
extern const uint8_t server_crt_end[]   asm("_binary_server_crt_end");
extern const uint8_t server_key_start[] asm("_binary_server_key_start");
extern const uint8_t server_key_end[]   asm("_binary_server_key_end");

/*static*/ httpd_handle_t http_https = NULL; /* port 443  */
static httpd_handle_t http_http = NULL;  /* port 80   */

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

static void register_recovery_handler(httpd_handle_t h, void *ctx)
{
    httpd_uri_t recovery = {
        .uri = "/*", // catch everything
        .method = HTTP_GET,
        .handler = rest_recovery_handler,
        .user_ctx = ctx,
    };
    httpd_register_uri_handler(h, &recovery);
}

static void register_all_handlers(httpd_handle_t h, void *ctx)
{
    static const httpd_uri_t routes[] = {
        /* recovery */
        {.uri = "/recovery", .method = HTTP_GET, .handler = rest_recovery_handler},

        /* system */
        {.uri = "/api/system/info", .method = HTTP_GET, .handler = GET_system_info},
        {.uri = "/api/system", .method = HTTP_PATCH, .handler = PATCH_update_settings},
        {.uri = "/api/system", .method = HTTP_OPTIONS, .handler = handle_options_request},
        {.uri = "/api/system/restart", .method = HTTP_POST, .handler = POST_restart},
        {.uri = "/api/system/OTA", .method = HTTP_POST, .handler = POST_OTA_update},
        {.uri = "/api/system/OTAWWW", .method = HTTP_POST, .handler = POST_WWW_update},

        /* influx */
        {.uri = "/api/influx/info", .method = HTTP_GET, .handler = GET_influx_info},
        {.uri = "/api/influx", .method = HTTP_PATCH, .handler = PATCH_update_influx},

        /* swarm */
        {.uri = "/api/swarm/info", .method = HTTP_GET, .handler = GET_swarm},
        {.uri = "/api/swarm", .method = HTTP_PATCH, .handler = PATCH_update_swarm},
        {.uri = "/api/swarm", .method = HTTP_OPTIONS, .handler = handle_options_request},

        /* websocket */
        {.uri = "/api/ws", .method = HTTP_GET, .handler = echo_handler, .is_websocket = true},

        /* static files & 404 */
        {.uri = "/*", .method = HTTP_GET, .handler = rest_common_get_handler},
    };

    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); ++i) {
        httpd_uri_t entry = routes[i];
        entry.user_ctx    = ctx;    // attach ctx
        httpd_register_uri_handler(h, &entry);
    }
}

static esp_err_t start_https_server(void)
{
    httpd_ssl_config_t cfg = HTTPD_SSL_CONFIG_DEFAULT();
    cfg.httpd.uri_match_fn = httpd_uri_match_wildcard;
    cfg.httpd.stack_size = 12288;
    cfg.httpd.server_port = 443;
    cfg.httpd.max_uri_handlers = 20;
    cfg.httpd.lru_purge_enable = true;
    //cfg.httpd.enable_keep_alive = true;

    cfg.servercert = server_crt_start;
    cfg.servercert_len = server_crt_end - server_crt_start;
    cfg.prvtkey_pem = server_key_start;
    cfg.prvtkey_len = server_key_end - server_key_start;

    ESP_ERROR_CHECK(httpd_ssl_start(&http_https, &cfg));
    return ESP_OK;
}

static esp_err_t start_http_server(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.uri_match_fn = httpd_uri_match_wildcard;
    cfg.stack_size = 12288;
    cfg.server_port = 80;
    cfg.max_uri_handlers = 20;
    cfg.lru_purge_enable = true;
    //cfg.httpd.enable_keep_alive = true;

    ESP_ERROR_CHECK(httpd_start(&http_http, &cfg));
    return ESP_OK;
}

esp_err_t start_rest_server(void *pvParameters)
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

    rest_server_context_t *rest_context = (rest_server_context_t *) CALLOC(1, sizeof(rest_server_context_t));
    if (!rest_context) {
        ESP_LOGE(TAG, "No memory for rest context");
        return ESP_FAIL;
    }

    strlcpy(rest_context->base_path, base_path, sizeof(rest_context->base_path));

    // start both servers
    start_https_server();
    start_http_server();

    httpd_handle_t servers[] = {http_https, http_http};
    const size_t NUM_SERVERS = sizeof(servers) / sizeof(servers[0]);

    for (size_t i = 0; i < NUM_SERVERS; ++i) {
        httpd_handle_t h = servers[i];
        if (!h)
            continue; /* skip if start failed */

        if (enter_recovery)
            register_recovery_handler(h, rest_context);
        else
            register_all_handlers(h, rest_context);

        httpd_register_err_handler(h, HTTPD_404_NOT_FOUND, http_404_error_handler);
    }

    websocket_start();

    // Start the DNS server that will redirect all queries to the softAP IP
    dns_server_config_t dns_config = DNS_SERVER_CONFIG_SINGLE("*" /* all A queries */, "WIFI_AP_DEF" /* softAP netif ID */);
    start_dns_server(&dns_config);

    return ESP_OK;
}
