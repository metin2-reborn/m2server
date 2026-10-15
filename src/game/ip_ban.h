#pragma once

#include <netinet/in.h>

namespace metin2::game {

extern bool LoadBanIP(const char * filename);
extern bool IsBanIP(struct in_addr in);
}
