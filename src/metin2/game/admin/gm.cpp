#include <metin2/config/config.h>
#include <metin2/game/admin/gm.h>

#include <metin2/log/log.h>
#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>

#include <cstdint>
#include <cstring>
#include <map>
#include <string>

namespace metin2::game::inline admin {

    std::map<std::string, t_admin_info> map_gm;

    void gm_new_clear() {
        map_gm.clear();
    }

    void gm_new_insert(const t_admin_info &admin_info) {
        metin2::log::info("Gm registered: account {}, player {}, contact ip {}, server ip {}, authority {:d}",
            admin_info.account,
            admin_info.name,
            admin_info.contact_ip,
            admin_info.server_ip,
            admin_info.authority);

        map_gm[admin_info.name] = admin_info;
    }

    auto gm_get_level(const char *name, const char *account) -> uint8_t {
        if (metin2::config::test_server()) return GM_IMPLEMENTOR;

        auto it = map_gm.find(name);

        if (map_gm.end() == it)
            return GM_PLAYER;

        if (account && strcmp(it->second.account, account) != 0) {
            metin2::log::warn("Gm {} logged in from account {} instead of {}", name, account, it->second.account);
            return GM_PLAYER;
        }

        metin2::log::debug("Gm {} has authority {:d}", name, it->second.authority);
        return it->second.authority;
    }
}
