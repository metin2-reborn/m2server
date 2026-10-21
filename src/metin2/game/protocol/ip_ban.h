#pragma once

#include <netinet/in.h>

namespace metin2::game::inline protocol {

    extern auto load_ban_ip(const char *filename) -> bool;
    extern auto is_ban_ip(struct in_addr in) -> bool;
}
