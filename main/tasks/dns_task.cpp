#include "dns_task.h"
#include "esp_log.h"
#include <lwip/inet.h>
#include <netdb.h>
#include <ctime>

static const char* TAG = "dns_task";

// DNS cache state
static std::string cached_host;
static ip_addr_t cached_ip;
static bool has_cached_ip = false;
static time_t last_resolve_time = 0;

#ifndef DNS_CACHE_TTL_SECONDS
#define DNS_CACHE_TTL_SECONDS 3600
#endif

bool resolve_hostname(const std::string& hostnameOrIp, ip_addr_t* outIp) {
    time_t now = time(NULL);

    if (has_cached_ip &&
        cached_host == hostnameOrIp &&
        difftime(now, last_resolve_time) <= DNS_CACHE_TTL_SECONDS) {

        *outIp = cached_ip;
        return true;
    }

    // Check if input is already an IP address
    ip_addr_t parsed_ip;
    if (ipaddr_aton(hostnameOrIp.c_str(), &parsed_ip)) {
        cached_ip = parsed_ip;
        cached_host = hostnameOrIp;
        last_resolve_time = now;
        has_cached_ip = true;
        *outIp = cached_ip;
        ESP_LOGI(TAG, "Parsed direct IP address: %s", hostnameOrIp.c_str());
        return true;
    }

    // Fallback to DNS resolution
    struct addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* res;
    int err = getaddrinfo(hostnameOrIp.c_str(), nullptr, &hints, &res);
    if (err != 0 || !res) {
        ESP_LOGE(TAG, "DNS lookup failed for '%s': %s", hostnameOrIp.c_str(), strerror(err));
        return false;
    }

    struct sockaddr_in* addr_in = (struct sockaddr_in*)res->ai_addr;
    ip4_addr_t ip4;
    ip4.addr = addr_in->sin_addr.s_addr;
    ip_addr_set_ip4_u32(&cached_ip, ip4.addr);
    cached_ip.type = IPADDR_TYPE_V4;

    freeaddrinfo(res);

    cached_host = hostnameOrIp;
    last_resolve_time = now;
    has_cached_ip = true;

    *outIp = cached_ip;
    ESP_LOGI(TAG, "Resolved %s to IP: %s", hostnameOrIp.c_str(), inet_ntoa(addr_in->sin_addr));
    return true;
}
