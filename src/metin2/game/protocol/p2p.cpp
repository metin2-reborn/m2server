#include <metin2/config/config.h>
#include <metin2/game/protocol/p2p.h>

#include <metin2/game/app/typedef.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/guilds/guild_manager.h>
#include <metin2/game/parties/party.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/social/marriage.h>
#include <metin2/game/social/messenger_manager.h>
#include <metin2/game/world/sectree_manager.h>

#include <metin2/log/log.h>
#include <metin2/proto/limits.h>

#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>

namespace metin2::game::inline protocol {

    p2p_manager::p2p_manager() {
        handle_count_ = 0;

        memset(empire_user_count_, 0, sizeof(empire_user_count_));
    }

    p2p_manager::~p2p_manager() = default;

    void p2p_manager::boot(desc *d) {
        character_manager::name_map &map = get_character_manager().pc_map();
        auto it = map.begin();

        packet_gg_login p;

        while (it != map.end()) {
            character *ch = it->second;
            it++;

            p.header = HEADER_GG_LOGIN;
            strlcpy(p.name, ch->name(), sizeof(p.name));
            p.pid = ch->player_id();
            p.empire = ch->empire();
            p.map_index = get_sectree_manager().map_index(ch->x(), ch->y());
            p.channel = metin2::config::server().channel;

            d->packet(&p, sizeof(p));
        }
    }

    void p2p_manager::flush_output() {
        auto it = peers_.begin();

        while (it != peers_.end()) {
            desc *desc = *it++;
            desc->flush_output();
        }
    }

    void p2p_manager::register_acceptor(desc *d) {
        metin2::log::info("Peer acceptor opened (host {})", d->host_name());
        peers_.insert(d);
        boot(d);
    }

    void p2p_manager::unregister_acceptor(desc *d) {
        metin2::log::info("Peer acceptor closed (host {})", d->host_name());
        erase_user_by_desc(d);
        peers_.erase(d);
    }

    void p2p_manager::register_connector(desc *d) {
        metin2::log::info("Peer connector opened (host {})", d->host_name());
        peers_.insert(d);
        boot(d);

        packet_gg_setup p;
        p.header = HEADER_GG_SETUP;
        p.port = metin2::config::server().p2p_port;
        p.channel = metin2::config::server().channel;
        d->packet(&p, sizeof(p));
    }

    void p2p_manager::unregister_connector(desc *d) {
        auto it = peers_.find(d);

        if (it != peers_.end()) {
            metin2::log::info("Peer connector closed (host {})", d->host_name());
            erase_user_by_desc(d);
            peers_.erase(it);
        }
    }

    void p2p_manager::erase_user_by_desc(desc *d) {
        auto it = ccis_.begin();

        while (it != ccis_.end()) {
            cci *cci = it->second.get();
            it++;

            if (cci->desc == d)
                logout(cci);
        }
    }

    void p2p_manager::send(const void *data, int size, desc *except) {
        auto it = peers_.begin();

        while (it != peers_.end()) {
            desc *desc = *it++;

            if (except == desc)
                continue;

            desc->packet(data, size);
        }
    }

    void p2p_manager::login(desc *d, const packet_gg_login *p) {
        cci *cci_value = find(p->name);

        bool update_p2_p = false;

        if (nullptr == cci_value) {
            update_p2_p = true;
            auto new_cci = std::make_unique<cci>();
            cci_value = new_cci.get();

            strlcpy(cci_value->name, p->name, sizeof(cci_value->name));

            cci_value->pid = p->pid;
            cci_value->empire = p->empire;

            if (p->channel == metin2::config::server().channel) {
                if (cci_value->empire < EMPIRE_MAX_NUM) {
                    ++empire_user_count_[cci_value->empire];
                } else {
                    metin2::log::error("Peer login with invalid empire {:d} (max {:d})", cci_value->empire, EMPIRE_MAX_NUM);
                }
            }

            pid_pk_ccis_.insert(std::make_pair(cci_value->pid, cci_value));
            ccis_.emplace(cci_value->name, std::move(new_cci));
        }

        cci_value->map_index = p->map_index;
        cci_value->desc = d;
        cci_value->channel = p->channel;
        metin2::log::debug("Peer login: player {}", cci_value->name);

        get_guild_manager().p2p_login_member(cci_value->pid);
        get_party_manager().p2p_login(cci_value->pid, cci_value->name);

        if (update_p2_p) {
            std::string name(cci_value->name);
            get_messenger_manager().p2p_login(name);
        }
    }

    void p2p_manager::logout(cci *cci_value) {
        if (cci_value->channel == metin2::config::server().channel) {
            if (cci_value->empire < EMPIRE_MAX_NUM) {
                --empire_user_count_[cci_value->empire];
                if (empire_user_count_[cci_value->empire] < 0) {
                    metin2::log::error("User count of empire {:d} dropped below zero", cci_value->empire);
                }
            } else {
                metin2::log::error("Peer logout with invalid empire {:d} (max {:d})", cci_value->empire, EMPIRE_MAX_NUM);
            }
        }

        std::string name(cci_value->name);

        get_guild_manager().p2p_logout_member(cci_value->pid);
        get_party_manager().p2p_logout(cci_value->pid);
        get_messenger_manager().p2p_logout(name);
        marriage::get_manager().logout(cci_value->pid);

        pid_pk_ccis_.erase(cci_value->pid);
        ccis_.erase(name);
    }

    void p2p_manager::logout(const char *name) {
        cci *cci = find(name);

        if (!cci)
            return;

        metin2::log::debug("Peer logout: player {}", name);
        logout(cci);
    }

    auto p2p_manager::find_by_pid(uint32_t pid) -> cci * {
        auto it = pid_pk_ccis_.find(pid);
        if (it == pid_pk_ccis_.end())
            return nullptr;
        return it->second;
    }

    auto p2p_manager::find(const char *name) -> cci * {
        cci_map::const_iterator it;

        it = ccis_.find(name);

        if (it == ccis_.end())
            return nullptr;

        return it->second.get();
    }

    auto p2p_manager::count() -> int {
        return empire_user_count_[1] + empire_user_count_[2] + empire_user_count_[3];
    }

    auto p2p_manager::empire_user_count(int idx) -> int {
        assert(idx < EMPIRE_MAX_NUM);
        return empire_user_count_[idx];
    }

    auto p2p_manager::desc_count() -> int {
        return peers_.size();
    }

    void p2p_manager::p2p_host_names(std::string &host_names) {
        auto it = peers_.begin();

        std::ostringstream oss(std::ostringstream::out);

        while (it != peers_.end()) {
            desc *desc = *it++;

            oss << desc->p2p_host() << " " << desc->p2p_port() << "\n";
        }
        host_names += oss.str();
    }
}
