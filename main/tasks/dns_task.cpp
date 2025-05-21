// dns_task.cpp
// Central DNS resolver with small cache, safe for use in multiple FreeRTOS tasks
// No dynamic STL usage, suitable for embedded environments

#include "dns_task.h"

#include "esp_log.h"
#include "esp_timer.h"

#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/inet.h"
#include "string.h"
#include "freertos/semphr.h"

#define DNS_CACHE_SIZE 2                         // Maximum number of cache entries
#define DNS_CACHE_TTL_US (60 * 60 * 1000000ULL)  // Cache entry lifetime: 60 minutes

static const char* TAG = "dns_task";

// Structure representing a DNS cache entry
typedef struct {
    bool valid;                                  // Entry validity
    char hostname[64];                           // Hostname associated with IP
    ip_addr_t ip;                                // Cached IP address
    uint64_t timestamp_us;                       // Time when this entry was added
} dns_cache_entry_t;

// DNS cache and mutex
static dns_cache_entry_t dns_cache[DNS_CACHE_SIZE];
static SemaphoreHandle_t dns_cache_mutex = NULL;

// Initializes the DNS subsystem: must be called once before usage
void dns_task_init()
{
    if (dns_cache_mutex == NULL) {
        dns_cache_mutex = xSemaphoreCreateMutex();
    }

    for (int i = 0; i < DNS_CACHE_SIZE; ++i) {
        dns_cache[i].valid = false;
    }
}

// Resolves a hostname or IP string to an ip_addr_t (IPv4)
// Returns true on success, false on failure
bool resolve_hostname(const char* hostnameOrIp, ip_addr_t* outIp)
{
    if (hostnameOrIp == NULL || outIp == NULL) {
        return false;
    }

    uint64_t now_us = esp_timer_get_time();

    // Try parsing direct IPv4 address
    if (ipaddr_aton(hostnameOrIp, outIp)) {
        ESP_LOGI(TAG, "Direct IP address detected: %s", hostnameOrIp);
        return true;
    }

    // Search cache for valid, non-expired entry
    if (dns_cache_mutex && xSemaphoreTake(dns_cache_mutex, pdMS_TO_TICKS(100))) {
        for (int i = 0; i < DNS_CACHE_SIZE; ++i) {
            if (dns_cache[i].valid &&
                strcmp(dns_cache[i].hostname, hostnameOrIp) == 0 &&
                (now_us - dns_cache[i].timestamp_us) < DNS_CACHE_TTL_US) {
                *outIp = dns_cache[i].ip;
                xSemaphoreGive(dns_cache_mutex);
                ESP_LOGI(TAG, "Cache hit: %s -> %s", hostnameOrIp, ipaddr_ntoa(&dns_cache[i].ip));
                return true;
            }
        }
        xSemaphoreGive(dns_cache_mutex);
    }

    // Not found in cache: perform DNS resolution
    struct addrinfo hints = {};
    hints.ai_family = AF_INET;  // IPv4 only

    struct addrinfo* res = NULL;
    int err = getaddrinfo(hostnameOrIp, NULL, &hints, &res);
    if (err != 0 || res == NULL) {
        ESP_LOGW(TAG, "DNS lookup failed for host: %s", hostnameOrIp);
        return false;
    }

    struct sockaddr_in* addr = (struct sockaddr_in*)res->ai_addr;
    ip4_addr_set_u32(&outIp->u_addr.ip4, addr->sin_addr.s_addr);
    outIp->type = IPADDR_TYPE_V4;
    freeaddrinfo(res);

    // Store result in cache
    if (dns_cache_mutex && xSemaphoreTake(dns_cache_mutex, pdMS_TO_TICKS(100))) {
        int slot = 0;
        uint64_t oldest = dns_cache[0].timestamp_us;

        // Select oldest or invalid slot
        for (int i = 0; i < DNS_CACHE_SIZE; ++i) {
            if (!dns_cache[i].valid) {
                slot = i;
                break;
            }
            if (dns_cache[i].timestamp_us < oldest) {
                slot = i;
                oldest = dns_cache[i].timestamp_us;
            }
        }

        strncpy(dns_cache[slot].hostname, hostnameOrIp, sizeof(dns_cache[slot].hostname) - 1);
        dns_cache[slot].hostname[sizeof(dns_cache[slot].hostname) - 1] = '\0';
        dns_cache[slot].ip = *outIp;
        dns_cache[slot].timestamp_us = now_us;
        dns_cache[slot].valid = true;

        xSemaphoreGive(dns_cache_mutex);
    }

    ESP_LOGI(TAG, "Resolved via DNS: %s -> %s", hostnameOrIp, ipaddr_ntoa(outIp));
    return true;
}
