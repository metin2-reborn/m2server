#pragma once

#include <span>
#include <string>
#include <string_view>

namespace metin2::inline common {

    // Names are resolved on acceptance so recreated containers do not retain stale IPs.
    auto is_allowed_peer(std::string_view address, std::span<const std::string> allowed_hosts, std::string_view local_address) -> bool;
}
