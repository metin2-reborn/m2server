#pragma once

#include <netinet/in.h>

namespace metin2::game::inline protocol {

extern bool load_ban_ip(const char * filename);
extern bool is_ban_ip(struct in_addr in);
}
