#include "dns_task.h"
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <esp_log.h>
#include <time.h>
#include <string>
#include <map>

static const char* TAG = "dns_task";

#ifndef DNS_CACHE_TTL_SECONDS
#define DNS_CACHE_TTL_SECONDS 3600
#endif

struct CachedEntry {
    ip_addr_t ip;
    time_t timestamp;
};

static std::map<std::string, CachedEntry> dns_cache;

bool resolve_hostname(const std::string& hostnameOrIp, ip_addr_t* outIp) {
    time_t now = time(NULL);

    // Check if input is a direct IP
    ip_addr_t parsed_ip;
    if (ipaddr_aton(hostnameOrIp.c_str(), &parsed_ip)) {
        *outIp = parsed_ip;
        ESP_LOGI(TAG, "Parsed direct IP address: %s", hostnameOrIp.c_str());
        return true;
    }

    // Lookup in cache
    auto it = dns_cache.find(hostnameOrIp);
    if (it != dns_cache.end()) {
        double age = difftime(now, it->second.timestamp);
        if (age <= DNS_CACHE_TTL_SECONDS) {
            *outIp = it->second.ip;
            ESP_LOGI(TAG, "Cache hit for %s -> %s", hostnameOrIp.c_str(), ipaddr_ntoa(&it->second.ip));
            return true;
        } else {
            dns_cache.erase(it);
        }
    }

    // Perform DNS lookup
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

    ip_addr_t result_ip;
    ip_addr_set_ip4_u32(&result_ip, ip4.addr);
    result_ip.type = IPADDR_TYPE_V4;

    freeaddrinfo(res);

    // Cache the result
    dns_cache[hostnameOrIp] = { result_ip, now };

    *outIp = result_ip;
    ESP_LOGI(TAG, "Resolved %s to IP: %s", hostnameOrIp.c_str(), inet_ntoa(addr_in->sin_addr));
    return true;
}
