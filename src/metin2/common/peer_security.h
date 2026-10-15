#pragma once

#include <span>
#include <string>

namespace metin2::inline common {

// Names are resolved on acceptance so recreated containers do not retain stale IPs.
auto is_allowed_peer(const char *address, std::span<const std::string> allowed_hosts, const char *local_address) -> bool;
}
