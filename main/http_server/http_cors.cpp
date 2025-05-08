#include "esp_http_server.h"
#include "esp_log.h"

#include "http_cors.h"
#include "global_state.h"

static const char* CORS_TAG = "http_cors";

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

esp_err_t is_network_allowed(httpd_req_t * req)
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

esp_err_t set_cors_headers(httpd_req_t *req)
{

    return httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*") == ESP_OK &&
                   httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, PUT, PATCH, DELETE, OPTIONS") == ESP_OK &&
                   httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type") == ESP_OK
               ? ESP_OK
               : ESP_FAIL;
}