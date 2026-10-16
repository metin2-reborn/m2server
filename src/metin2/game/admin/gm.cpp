#include <metin2/config/config.h>
#include <metin2/game/admin/gm.h>

#include <metin2/log/log.h>
#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>

#include <cstdint>
#include <cstring>
#include <map>
#include <set>
#include <string>

namespace metin2::game::inline admin {

    struct t_gm {
        t_admin_info info;
        std::set<std::string> *pset_host;
    };

    std::set<std::string> set_host;
    std::map<std::string, t_gm> map_gm;

    void gm_new_clear() {
        set_host.clear();
        map_gm.clear();
    }

    void gm_new_insert(const t_admin_info &admin_info) {
        metin2::log::info("Gm registered: account {}, player {}, contact ip {}, server ip {}, authority {:d}",
            admin_info.account,
            admin_info.name,
            admin_info.contact_ip,
            admin_info.server_ip,
            admin_info.authority);

        t_gm t;

        if (strlen(admin_info.contact_ip) == 0) {
            t.pset_host = &set_host;
            metin2::log::debug("Gm {} uses the gm host list", admin_info.name);
        } else {
            t.pset_host = nullptr;
            metin2::log::debug("Gm {} uses its own contact ip", admin_info.name);
        }

        memcpy(&t.info, &admin_info, sizeof(admin_info));

        map_gm[admin_info.name] = t;
    }

    void gm_new_host_inert(const char *host) {
        set_host.insert(host);
        metin2::log::debug("Gm host added: {}", host);
    }

    auto gm_new_get_level(const char *name, const char *host, const char *account) -> uint8_t {
        if (metin2::config::test_server()) return GM_IMPLEMENTOR;

        std::map<std::string, t_gm>::iterator it = map_gm.find(name);

        if (map_gm.end() == it)
            return GM_PLAYER;

        if (account) {
            if (strcmp(it->second.info.account, account) != 0) {
                metin2::log::warn("Gm {} logged in from account {} instead of {}", name, account, it->second.info.account);
                return GM_PLAYER;
            }
        }
        metin2::log::debug("Gm {} has authority {:d}", name, it->second.info.authority);
        return it->second.info.authority;
        return GM_PLAYER;
    }

    auto gm_get_level(const char *name, const char *host, const char *account) -> uint8_t {
        return gm_new_get_level(name, host, account);
    }
}
