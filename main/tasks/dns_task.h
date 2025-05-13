#pragma once

#include "lwip/ip_addr.h"
#include <string>

/**
 * Resolves a hostname to an IP address (with DNS cache and TTL).
 * If input is already an IP, it just parses and returns it.
 * 
 * @param hostnameOrIp FQDN or IPv4 address string
 * @param outIp         Output IP address (IPv4 only)
 * @return true if resolution was successful
 */
bool resolve_hostname(const std::string& hostnameOrIp, ip_addr_t* outIp);
