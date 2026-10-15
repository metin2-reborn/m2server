#pragma once

#include <netinet/in.h>

extern bool LoadBanIP(const char * filename);
extern bool IsBanIP(struct in_addr in);
