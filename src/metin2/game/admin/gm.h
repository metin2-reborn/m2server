#include <metin2/proto/tables.h>

#include <cstdint>

#pragma once

namespace metin2::game::inline admin {

    extern auto gm_get_level(const char *name, const char *host = nullptr, const char *account = nullptr) -> uint8_t;
    extern void gm_new_clear();
    extern void gm_new_insert(const t_admin_info &admin_info);
    extern void gm_new_host_inert(const char *host);
}
