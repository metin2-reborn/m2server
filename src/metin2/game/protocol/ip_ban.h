#pragma once

#include <netinet/in.h>

namespace metin2::game::inline protocol {

extern bool LoadBanIP(const char * filename);
extern bool IsBanIP(struct in_addr in);
}
