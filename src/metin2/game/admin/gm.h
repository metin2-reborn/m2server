#include <metin2/proto/tables.h>

#include <cstdint>

#pragma once

namespace metin2::game::inline admin {

extern uint8_t gm_get_level(const char * name, const char * host = nullptr, const char * account = nullptr);
extern void gm_new_clear();
extern void gm_new_insert( const t_admin_info & admin_info );
extern void gm_new_host_inert( const char * host );
}
