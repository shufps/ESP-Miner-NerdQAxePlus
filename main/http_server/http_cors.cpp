#include "http_cors.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "global_state.h"

static const char *CORS_TAG = "http_cors";

// Returns true if the IPv4 address (in network byte order) is in RFC1918 private ranges
static bool ip_in_private_range(uint32_t address_be)
{
    uint32_t ip = ntohl(address_be); // convert to host byte order for compare

    // 10.0.0.0 - 10.255.255.255
    if (ip >= 0x0A000000 && ip <= 0x0AFFFFFF)
        return true;

    // 172.16.0.0 - 172.31.255.255
    if (ip >= 0xAC100000 && ip <= 0xAC1FFFFF)
        return true;

    // 192.168.0.0 - 192.168.255.255
    if (ip >= 0xC0A80000 && ip <= 0xC0A8FFFF)
        return true;

    return false;
}

// Parse dotted IPv4 string (no port) to network-byte-order uint32_t
static uint32_t get_origin_ip(const char *host_without_port)
{
    uint32_t origin_ip_addr = inet_addr(host_without_port); // network byte order
    if (origin_ip_addr == INADDR_NONE) {
        ESP_LOGW(CORS_TAG, "Invalid IP address: %s", host_without_port);
    } else {
        ESP_LOGI(CORS_TAG, "Extracted IP address %lu", origin_ip_addr);
    }
    return origin_ip_addr;
}

// Extract host from Origin header of form "http://host[:port]/..."
// inplace operation
static const char *extract_origin_host(char *origin)
{
    if (!origin)
        return NULL;

    const char *http_prefix = "http://";
    size_t http_len = strlen(http_prefix);

    // Only accept http:// (device does not serve https)
    if (strncmp(origin, http_prefix, http_len) != 0) {
        return NULL;
    }

    // Move past "http://"
    char *host = origin + http_len;

    // Cut at first slash to drop any path
    char *slash = strchr(host, '/');
    if (slash)
        *slash = '\0';

    // Cut optional :port (IPv4 / hostname form)
    char *colon = strchr(host, ':');
    if (colon)
        *colon = '\0';

    return host; // host is now [hostname OR IPv4 literal] without port/path
}

// Check "localhost"
static bool is_localhost(const char *host_wo_port)
{
    if (!host_wo_port)
        return false;
    return strcmp(host_wo_port, "localhost") == 0;
}

// Check "*.local"
static bool is_local(const char *host_wo_port)
{
    if (!host_wo_port)
        return false;

    const char *suffix = ".local";
    size_t len = strlen(host_wo_port);
    size_t suf_len = strlen(suffix);

    if (len < suf_len)
        return false;

    return strcmp(host_wo_port + len - suf_len, suffix) == 0;
}

esp_err_t is_network_allowed(httpd_req_t *req)
{
    // AP mode: always allow
    if (SYSTEM_MODULE.getAPState()) {
        ESP_LOGI(CORS_TAG, "Device in AP mode. Allowing CORS.");
        return ESP_OK;
    }

    // Determine client's IPv4 address
    int sockfd = httpd_req_to_sockfd(req);
    struct sockaddr_in6 addr;   // esp_http_server uses IPv6 addressing
    socklen_t addr_size = sizeof(addr);

    if (getpeername(sockfd, (struct sockaddr *) &addr, &addr_size) < 0) {
        ESP_LOGE(CORS_TAG, "Error getting client IP");
        return ESP_FAIL;
    }

    uint32_t request_ip_addr = addr.sin6_addr.un.u32_addr[3];

    char ipstr[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET, &request_ip_addr, ipstr, sizeof(ipstr));
    ESP_LOGI(CORS_TAG, "Client IP: %s", ipstr);

    // Try to read Origin header (browser sends this on CORS requests)
    char origin[128];
    uint32_t origin_ip_addr;

    if (httpd_req_get_hdr_value_str(req, "Origin", origin, sizeof(origin)) == ESP_OK) {
        ESP_LOGI(CORS_TAG, "Origin header: %s", origin);

        const char *host = extract_origin_host(origin);
        if (!host) {
            ESP_LOGW(CORS_TAG, "Couldn't extract origin host from: %s", origin);
            return ESP_FAIL;
        }
        ESP_LOGI(CORS_TAG, "Extracted origin host: %s", host);

        // Check against device hostname (match exact, no port)
        const char *hostname = SYSTEM_MODULE.getHostname();
        if (strcmp(host, hostname) == 0) {
            ESP_LOGI(CORS_TAG, "allowed: origin matches hostname");
            return ESP_OK;
        }

        // Allow localhost / *.local cases
        if (is_localhost(host) || is_local(host)) {
            ESP_LOGI(CORS_TAG, "allowed: localhost or .local origin");
            return ESP_OK;
        }

        // Else, interpret host as IPv4 literal (must succeed to continue)
        origin_ip_addr = get_origin_ip(host);

    } else {
        // No Origin header → treat the client IP as origin
        ESP_LOGI(CORS_TAG, "No Origin header found, assuming direct same-client access.");
        origin_ip_addr = request_ip_addr;
    }

    // Final rule: both request IP and origin IP must be private RFC1918
    if (ip_in_private_range(origin_ip_addr) && ip_in_private_range(request_ip_addr)) {
        ESP_LOGI(CORS_TAG, "allowed: both IPs private");
        return ESP_OK;
    }

    ESP_LOGE(CORS_TAG, "denied: not in private range / mismatch");
    return ESP_FAIL;
}

esp_err_t set_cors_headers(httpd_req_t *req)
{
    return (httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*") == ESP_OK &&
            httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, PUT, PATCH, DELETE, OPTIONS") == ESP_OK &&
            httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type") == ESP_OK)
               ? ESP_OK
               : ESP_FAIL;
}
