// dns_task.h
// DNS resolver interface for ESP32 project
// Provides hostname resolution with simple caching (IPv4 only)
// Designed for use by multiple tasks (e.g. ping_task, stratum_task)

#ifndef DNS_TASK_H
#define DNS_TASK_H

#include "lwip/ip_addr.h"
#include <stdbool.h>

// Initializes the DNS resolver and internal cache
// Must be called before any use of resolve_hostname()
void dns_task_init(void);

// Resolves a hostname or direct IP string to an ip_addr_t
// Returns true on success, false if resolution fails
bool resolve_hostname(const char* hostnameOrIp, ip_addr_t* outIp);

#endif // DNS_TASK_H
