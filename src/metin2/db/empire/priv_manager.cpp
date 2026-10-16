#include <metin2/db/empire/priv_manager.h>

#include <metin2/db/clients/client_manager.h>
#include <metin2/db/clients/peer.h>

#include <metin2/log/log.h>
#include <metin2/proto/limits.h>
#include <metin2/proto/log_types.h>
#include <metin2/proto/tables.h>

#include <cstdint>
#include <ctime>
#include <utility>

namespace metin2::db::inline empire {

    const int CHARACTER_GOOD_PRIV_DURATION = 2 * 60 * 60;
    const int CHARACTER_BAD_PRIV_DURATION = 60 * 60;

    priv_manager::priv_manager() {
        for (int type = 0; type < MAX_PRIV_NUM; ++type) {
            for (int empire = 0; empire < EMPIRE_MAX_NUM; ++empire)
                aa_priv_empire_[type][empire] = nullptr;
        }
    }

    void priv_manager::update() {
        time_t now = get_client_manager().current_time();

        while (!pq_priv_guild_.empty() && pq_priv_guild_.top().first <= now) {
            auto p = pq_priv_guild_.top().second;
            pq_priv_guild_.pop();

            if (p->value != 0 && !p->removed) {

                auto it = priv_guild_[p->type].find(p->guild_id);

                if (it != priv_guild_[p->type].end() && it->second == p.get()) {
                    priv_guild_[p->type].erase(it);
                    send_change_guild_priv(p->guild_id, p->type, 0, 0);
                }
            }
        }

        while (!pq_priv_empire_.empty() && pq_priv_empire_.top().first <= now) {
            auto p = pq_priv_empire_.top().second;
            pq_priv_empire_.pop();

            if (p->value != 0 && !p->removed) {
                send_change_empire_priv(p->empire, p->type, 0, 0);
                aa_priv_empire_[p->type][p->empire] = nullptr;
            }
        }

        while (!pq_priv_char_.empty() && pq_priv_char_.top().first <= now) {
            auto p = pq_priv_char_.top().second;
            pq_priv_char_.pop();

            if (!p->removed) {
                send_change_char_priv(p->pid, p->type, 0);
                auto it = priv_char_[p->type].find(p->pid);
                if (it != priv_char_[p->type].end())
                    priv_char_[p->type].erase(it);
            }
        }
    }

    void priv_manager::add_char_priv(uint32_t pid, uint8_t type, int value) {
        if (MAX_PRIV_NUM <= type) {
            metin2::log::warn("Received invalid player privilege type {:d}", type);
            return;
        }

        auto it = priv_char_[type].find(pid);

        if (it != priv_char_[type].end())
            return;

        if (!value)
            return;

        time_t now = get_client_manager().current_time();
        auto p = std::make_shared<priv_char_data>(type, value, pid);

        int duration = CHARACTER_BAD_PRIV_DURATION;

        if (value > 0)
            duration = CHARACTER_GOOD_PRIV_DURATION;

        pq_priv_char_.push(std::make_pair(now + duration, p));
        priv_char_[type].insert(std::make_pair(pid, p.get()));

        metin2::log::info("Privilege: player pid {:d} type {:d} value {:d}", pid, type, value);
        send_change_char_priv(pid, type, value);
    }

    void priv_manager::add_guild_priv(uint32_t guild_id, uint8_t type, int value, time_t duration_sec) {
        if (MAX_PRIV_NUM <= type) {
            metin2::log::warn("Received invalid guild privilege type {:d}", type);
            return;
        }

        auto it = priv_guild_[type].find(guild_id);

        time_t now = get_client_manager().current_time();
        time_t end = now + duration_sec;
        auto p = std::make_shared<priv_guild_data>(type, value, guild_id, end);
        pq_priv_guild_.push(std::make_pair(end, p));

        if (it != priv_guild_[type].end())
            it->second = p.get();
        else
            priv_guild_[type].insert(std::make_pair(guild_id, p.get()));

        send_change_guild_priv(guild_id, type, value, end);

        metin2::log::info("Privilege: guild {:d} type {:d} value {:d} for {:d} s", guild_id, type, value, duration_sec);
    }

    void priv_manager::add_empire_priv(uint8_t empire, uint8_t type, int value, time_t duration_sec) {
        if (MAX_PRIV_NUM <= type || EMPIRE_MAX_NUM <= empire) {
            metin2::log::warn("Received invalid empire privilege type {:d} for empire {:d}", type, empire);
            return;
        }

        if (duration_sec < 0)
            duration_sec = 0;

        time_t now = get_client_manager().current_time();
        time_t end = now + duration_sec;

        {
            if (aa_priv_empire_[type][empire])
                aa_priv_empire_[type][empire]->removed = true;
        }

        auto p = std::make_shared<priv_empire_data>(type, value, empire, end);
        pq_priv_empire_.push(std::make_pair(end, p));
        aa_priv_empire_[type][empire] = p.get();

        send_change_empire_priv(empire, type, value, end);

        metin2::log::info("Privilege: empire {:d} type {:d} value {:d} for {:d} s", empire, type, value, duration_sec);
    }

    namespace {
        auto change_guild_priv_sender(uint32_t guild_id, uint8_t type, int value, time_t end_time_sec) {
            packet_dg_change_guild_priv p;
            p.guild_id = guild_id;
            p.type = type;
            p.value = value;
            p.log = 1;
            p.end_time_sec = end_time_sec;

            return [p](peer *peer) mutable {
                peer->encode_header(HEADER_DG_CHANGE_GUILD_PRIV, 0, sizeof(packet_dg_change_guild_priv));
                peer->encode(&p, sizeof(packet_dg_change_guild_priv));
                p.log = 0;
            };
        }

        auto change_empire_priv_sender(uint8_t empire, uint8_t type, int value, time_t end_time_sec) {
            packet_dg_change_empire_priv p;
            p.empire = empire;
            p.type = type;
            p.value = value;
            p.log = 1;
            p.end_time_sec = end_time_sec;

            return [p](peer *peer) mutable {
                peer->encode_header(HEADER_DG_CHANGE_EMPIRE_PRIV, 0, sizeof(packet_dg_change_empire_priv));
                peer->encode(&p, sizeof(packet_dg_change_empire_priv));
                p.log = 0;
            };
        }

        auto change_char_priv_sender(uint32_t pid, uint8_t type, int value) {
            packet_dg_change_character_priv p;
            p.pid = pid;
            p.type = type;
            p.value = value;
            p.log = 1;

            return [p](peer *peer) mutable {
                peer->encode_header(HEADER_DG_CHANGE_CHARACTER_PRIV, 0, sizeof(packet_dg_change_character_priv));
                peer->encode(&p, sizeof(packet_dg_change_character_priv));
                p.log = 0;
            };
        }
    }

    void priv_manager::send_change_guild_priv(uint32_t guild_id, uint8_t type, int value, time_t end_time_sec) {
        get_client_manager().for_each_peer(change_guild_priv_sender(guild_id, type, value, end_time_sec));
    }

    void priv_manager::send_change_empire_priv(uint8_t empire, uint8_t type, int value, time_t end_time_sec) {
        get_client_manager().for_each_peer(change_empire_priv_sender(empire, type, value, end_time_sec));
    }

    void priv_manager::send_change_char_priv(uint32_t pid, uint8_t type, int value) {
        get_client_manager().for_each_peer(change_char_priv_sender(pid, type, value));
    }

    void priv_manager::send_priv_on_setup(peer *peer) {
        for (int i = 1; i < MAX_PRIV_NUM; ++i) {
            for (int e = 0; e < EMPIRE_MAX_NUM; ++e) {
                priv_empire_data *priv_empire_data = aa_priv_empire_[i][e];
                if (priv_empire_data) {
                    change_empire_priv_sender(e, i, priv_empire_data->value, priv_empire_data->end_time_sec)(peer);
                }
            }

            for (auto it = priv_guild_[i].begin(); it != priv_guild_[i].end(); ++it) {
                change_guild_priv_sender(it->first, i, it->second->value, it->second->end_time_sec)(peer);
            }
            for (auto it = priv_char_[i].begin(); it != priv_char_[i].end(); ++it) {
                change_char_priv_sender(it->first, i, it->second->value)(peer);
            }
        }
    }
}
