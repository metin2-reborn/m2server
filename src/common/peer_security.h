#pragma once

#include <string>

// Names are resolved on acceptance so recreated containers do not retain stale IPs.
auto is_allowed_peer(const char *address, const std::string &allowed_hosts, const char *local_address) -> bool;

extern std::string g_allowedPeerHosts;
