#include "dns_task.h"
#include "esp_log.h"
#include <lwip/inet.h>
#include <lwip/netdb.h>
#include <string.h>
#include <ctime>
#include <mutex>

#ifndef DNS_CACHE_TTL_SECONDS
#define DNS_CACHE_TTL_SECONDS 3600
#endif

#ifndef DNS_CACHE_SIZE
#define DNS_CACHE_SIZE 2
#endif

static const char* TAG = "dns_task";

// DNS cache structure for hostnames
struct CachedEntry {
    char hostname[64];     // assuming max 63 char hostnames
    ip_addr_t ip;
    time_t timestamp;
    bool valid;
};

static CachedEntry dns_cache[DNS_CACHE_SIZE];  // max DNS_CACHE_SIZE cached entries
static std::mutex dns_cache_mutex;

bool resolve_hostname(const std::string& hostnameOrIp, ip_addr_t* outIp) {
    time_t now = time(NULL);

    // Check if input is a direct IP address
    ip_addr_t parsed_ip;
    if (ipaddr_aton(hostnameOrIp.c_str(), &parsed_ip)) {
        *outIp = parsed_ip;
        ESP_LOGI(TAG, "Parsed direct IP address: %s", hostnameOrIp.c_str());
        return true;
    }

    // Look up in cache
    {
        std::lock_guard<std::mutex> lock(dns_cache_mutex);
        for (int i = 0; i < DNS_CACHE_SIZE; ++i) {
            if (dns_cache[i].valid &&
                strcmp(dns_cache[i].hostname, hostnameOrIp.c_str()) == 0 &&
                difftime(now, dns_cache[i].timestamp) <= DNS_CACHE_TTL_SECONDS) {
                
                *outIp = dns_cache[i].ip;
                ESP_LOGI(TAG, "Cache hit for %s -> %s", hostnameOrIp.c_str(), ipaddr_ntoa(&dns_cache[i].ip));
                return true;
            }
        }
    }

    // Perform DNS lookup
    struct addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* res = nullptr;
    int err = getaddrinfo(hostnameOrIp.c_str(), nullptr, &hints, &res);
    if (err != 0 || !res) {
        ESP_LOGE(TAG, "DNS lookup failed for '%s': %s", hostnameOrIp.c_str(), strerror(err));
        if (res) freeaddrinfo(res);
        return false;
    }

    // Extract IP address
    struct sockaddr_in* addr_in = (struct sockaddr_in*)res->ai_addr;
    ip4_addr_t ip4;
    ip4.addr = addr_in->sin_addr.s_addr;

    ip_addr_t result_ip;
    ip_addr_set_ip4_u32(&result_ip, ip4.addr);
    result_ip.type = IPADDR_TYPE_V4;

    freeaddrinfo(res);

    // Cache the result
    {
        std::lock_guard<std::mutex> lock(dns_cache_mutex);

        // Try to find existing entry slot or free one
        CachedEntry* target = nullptr;
        for (int i = 0; i < DNS_CACHE_SIZE; ++i) {
            if (!dns_cache[i].valid || strcmp(dns_cache[i].hostname, hostnameOrIp.c_str()) == 0) {
                target = &dns_cache[i];
                break;
            }
        }

        // If slots are used and different, overwrite the older one
        if (!target) {
            // Find the oldest entry to overwrite
            target = &dns_cache[0];
            for (int i = 1; i < DNS_CACHE_SIZE; ++i) {
                if (dns_cache[i].timestamp < target->timestamp) {
                    target = &dns_cache[i];
                }
            }
        }

        strncpy(target->hostname, hostnameOrIp.c_str(), sizeof(target->hostname) - 1);
        target->hostname[sizeof(target->hostname) - 1] = '\0';
        target->ip = result_ip;
        target->timestamp = now;
        target->valid = true;
    }

    *outIp = result_ip;
    ESP_LOGI(TAG, "Resolved %s to IP: %s", hostnameOrIp.c_str(), inet_ntoa(addr_in->sin_addr));
    return true;
}
