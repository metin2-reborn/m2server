#include <metin2/config/config.h>
#include <metin2/game/app/config.h>
#include <metin2/game/app/constants.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/combat/skill.h>
#include <metin2/game/guilds/guild.h>
#include <metin2/game/guilds/guild_manager.h>
#include <metin2/game/guilds/war_map.h>
#include <metin2/game/protocol/buffer_manager.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/protocol/p2p.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/quest/questpc.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/world/sectree_manager.h>

#include <metin2/log/log.h>
#include <metin2/proto/chat_types.h>
#include <metin2/proto/guild_types.h>
#include <metin2/proto/tables.h>

#include <cstdint>
#include <cstdio>
#include <utility>

namespace metin2::game::inline guilds {

    void guild::guild_war_packet(uint32_t opp_gid, uint8_t war_type, uint8_t war_state_value) {
        packet_gc_guild pack;
        packet_gc_guild_war pack2;

        pack.header = HEADER_GC_GUILD;
        pack.subheader = GUILD_SUBHEADER_GC_WAR;
        pack.size = sizeof(pack) + sizeof(pack2);
        pack2.guild_self = id();
        pack2.guild_opp = opp_gid;
        pack2.war_state = war_state_value;
        pack2.type = war_type;

        for (auto it = member_online_.begin(); it != member_online_.end(); ++it) {
            character *ch = *it;

            if (war_state_value == GUILD_WAR_ON_WAR)
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 길드전중에는 사냥에 따른 이익이 없습니다."));

            desc *d = ch->get_desc();

            if (d) {
                ch->send_guild_name(opp_gid);

                d->buffered_packet(&pack, sizeof(pack));
                d->packet(&pack2, sizeof(pack2));
            }
        }
    }

    void guild::send_enemy_guild(character *ch) {
        desc *d = ch->get_desc();

        if (!d)
            return;

        packet_gc_guild pack;
        packet_gc_guild_war pack2;
        pack.header = HEADER_GC_GUILD;
        pack.subheader = GUILD_SUBHEADER_GC_WAR;
        pack.size = sizeof(pack) + sizeof(pack2);
        pack2.guild_self = id();

        packet_gc_guild p;
        p.header = HEADER_GC_GUILD;
        p.subheader = GUILD_SUBHEADER_GC_WAR_SCORE;
        p.size = sizeof(p) + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(int32_t);

        for (auto it = enemy_guild_.begin(); it != enemy_guild_.end(); ++it) {
            ch->send_guild_name(it->first);

            pack2.guild_opp = it->first;
            pack2.type = it->second.type;
            pack2.war_state = it->second.state;

            d->buffered_packet(&pack, sizeof(pack));
            d->packet(&pack2, sizeof(pack2));

            if (it->second.state == GUILD_WAR_ON_WAR) {
                int32_t score;

                score = war_score_against_to(pack2.guild_opp);

                d->buffered_packet(&p, sizeof(p));
                d->buffered_packet(&pack2.guild_self, sizeof(uint32_t));
                d->buffered_packet(&pack2.guild_opp, sizeof(uint32_t));
                d->packet(&score, sizeof(int32_t));

                score = guild_manager::instance().touch_guild(pack2.guild_opp)->war_score_against_to(pack2.guild_self);

                d->buffered_packet(&p, sizeof(p));
                d->buffered_packet(&pack2.guild_opp, sizeof(uint32_t));
                d->buffered_packet(&pack2.guild_self, sizeof(uint32_t));
                d->packet(&score, sizeof(int32_t));
            }
        }
    }

    auto guild::get_guild_war_state(uint32_t opp_gid) -> int {
        if (opp_gid == id())
            return GUILD_WAR_NONE;

        auto it = enemy_guild_.find(opp_gid);
        return (it != enemy_guild_.end()) ? (it->second.state) : static_cast<uint32_t>(GUILD_WAR_NONE);
    }

    auto guild::get_guild_war_type(uint32_t opp_gid) -> int {
        auto git = enemy_guild_.find(opp_gid);

        if (git == enemy_guild_.end())
            return GUILD_WAR_TYPE_FIELD;

        return git->second.type;
    }

    auto guild::guild_war_map_index(uint32_t opp_gid) -> uint32_t {
        auto git = enemy_guild_.find(opp_gid);

        if (git == enemy_guild_.end())
            return 0;

        return git->second.map_index;
    }

    auto guild::can_start_war(uint8_t guild_war_type) -> bool {
        if (guild_war_type >= GUILD_WAR_TYPE_MAX_NUM)
            return false;

        if (metin2::config::test_server() || quest::quest_manager::instance().event_flag("guild_war_test") != 0)
            return ladder_point() > 0;

        return ladder_point() > 0 && member_count() >= GUILD_WAR_MIN_MEMBER_COUNT;
    }

    auto guild::under_war(uint32_t opp_gid) -> bool {
        if (opp_gid == id())
            return false;

        auto it = enemy_guild_.find(opp_gid);
        return (it != enemy_guild_.end()) && (it->second.is_war_begin());
    }

    auto guild::under_any_war(uint8_t type_value) -> uint32_t {
        for (auto it = enemy_guild_.begin(); it != enemy_guild_.end(); ++it) {
            if (type_value < GUILD_WAR_TYPE_MAX_NUM)
                if (it->second.type != type_value)
                    continue;

            if (it->second.is_war_begin())
                return it->first;
        }

        return 0;
    }

    void guild::set_war_score_against_to(uint32_t opp_gid, int score_value) {
        uint32_t self_gid = id();

        metin2::log::debug("Guild war: guild {:d} score against guild {:d} set to {:d}", self_gid, opp_gid, score_value);
        auto it = enemy_guild_.find(opp_gid);

        if (it != enemy_guild_.end()) {
            it->second.score = score_value;

            if (it->second.type != GUILD_WAR_TYPE_FIELD) {
                guild *g_opp = guild_manager::instance().touch_guild(opp_gid);
                war_map *map = war_map_manager::instance().find(it->second.map_index);

                if (map)
                    map->update_score(self_gid, score_value, opp_gid, g_opp->war_score_against_to(self_gid));
            } else {
                packet_gc_guild p;

                p.header = HEADER_GC_GUILD;
                p.subheader = GUILD_SUBHEADER_GC_WAR_SCORE;
                p.size = sizeof(p) + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(int32_t);

                temp_buffer buf;
                buf.write(&p, sizeof(p));

                buf.write(&self_gid, sizeof(uint32_t));
                buf.write(&opp_gid, sizeof(uint32_t));
                buf.write(&score_value, sizeof(int32_t));

                packet(buf.read_peek(), buf.size());

                guild *g_opp = guild_manager::instance().touch_guild(opp_gid);

                if (g_opp)
                    g_opp->packet(buf.read_peek(), buf.size());
            }
        }
    }

    auto guild::war_score_against_to(uint32_t opp_gid) -> int {
        auto it = enemy_guild_.find(opp_gid);

        if (it != enemy_guild_.end()) {
            metin2::log::debug("Guild war: guild {:d} score against guild {:d} is {:d}", id(), opp_gid, it->second.score);
            return it->second.score;
        }

        metin2::log::debug("Guild war: guild {:d} has no score against guild {:d}", id(), opp_gid);
        return 0;
    }

    auto guild::war_start_time(uint32_t opp_gid) -> uint32_t {
        if (opp_gid == id())
            return 0;

        auto it = enemy_guild_.find(opp_gid);

        if (it == enemy_guild_.end())
            return 0;

        return it->second.war_start_time;
    }

    auto guild_war_get_type_info(unsigned type) -> const guild_war_info & {
        return guild_war_types[type];
    }

    auto guild_war_get_type_map_index(unsigned type) -> unsigned {
        return guild_war_get_type_info(type).map_index;
    }

    auto guild_war_is_war_map(unsigned type) -> bool {
        if (type == GUILD_WAR_TYPE_FIELD)
            return true;

        unsigned map_index = guild_war_get_type_map_index(type);

        if (sectree_manager::instance().get_map_region(map_index))
            return true;

        return false;
    }

    void guild::notify_guild_master(const char *msg) {
        character *ch = master_character();
        if (ch)
            ch->chat_packet(CHAT_TYPE_INFO, msg);
    }

    void guild::request_declare_war(uint32_t opp_gid, uint8_t type) {
        if (opp_gid == id()) {
            metin2::log::warn("Guild {:d} tried to declare war on itself ({:d}), type {:d}", id(), opp_gid, type);
            return;
        }

        if (type >= GUILD_WAR_TYPE_MAX_NUM) {
            metin2::log::warn("Guild {:d} declared war on guild {:d} with unknown war type {:d}", id(), opp_gid, type);
            return;
        }

        auto it = enemy_guild_.find(opp_gid);
        if (it == enemy_guild_.end()) {
            if (!guild_war_is_war_map(type)) {
                metin2::log::error("Cannot declare war from guild {:d} on guild {:d}, war type {:d} has no war map {:d}",
                    id(), opp_gid, type, guild_war_get_type_map_index(type));

                map_allow_log();
                notify_guild_master(lc_text("전쟁 서버가 열려있지 않아 길드전을 시작할 수 없습니다."));
                return;
            }

            packet_guild_war p;
            p.type = type;
            p.war = GUILD_WAR_SEND_DECLARE;
            p.guild_from = id();
            p.guild_to = opp_gid;
            db_clientdesc->db_packet(HEADER_GD_GUILD_WAR, 0, &p, sizeof(p));
            metin2::log::info("Guild war: guild {:d} declared war on guild {:d}, type {:d}", id(), opp_gid, type);
            return;
        }

        switch (it->second.state) {
        case GUILD_WAR_RECV_DECLARE:
            {
                const unsigned saved_type = it->second.type;

                if (saved_type == GUILD_WAR_TYPE_FIELD) {
                    packet_guild_war p;
                    p.type = saved_type;
                    p.war = GUILD_WAR_ON_WAR;
                    p.guild_from = id();
                    p.guild_to = opp_gid;
                    db_clientdesc->db_packet(HEADER_GD_GUILD_WAR, 0, &p, sizeof(p));
                    metin2::log::info("Guild war: guild {:d} accepted war from guild {:d}, type {:d}", id(), opp_gid, saved_type);
                    return;
                }

                if (!guild_war_is_war_map(saved_type)) {
                    metin2::log::error("Cannot accept war for guild {:d} against guild {:d}, war type {:d} has no war map {:d}",
                        id(), opp_gid, type, guild_war_get_type_map_index(type));

                    map_allow_log();
                    notify_guild_master(lc_text("전쟁 서버가 열려있지 않아 길드전을 시작할 수 없습니다."));
                    return;
                }

                const guild_war_info &guild_war_info = guild_war_get_type_info(saved_type);

                packet_guild_war p;
                p.type = saved_type;
                p.war = GUILD_WAR_WAIT_START;
                p.guild_from = id();
                p.guild_to = opp_gid;
                p.war_price = guild_war_info.war_price;
                p.initial_score = guild_war_info.initial_score;

                if (metin2::config::test_server())
                    p.initial_score /= 10;

                db_clientdesc->db_packet(HEADER_GD_GUILD_WAR, 0, &p, sizeof(p));

                metin2::log::info("Guild war: guild {:d} vs {:d} waiting to start, type {:d}, bet {:d}, map {:d}",
                    id(), opp_gid, saved_type, guild_war_info.war_price, guild_war_info.map_index);
            }
            break;
        case GUILD_WAR_SEND_DECLARE:
            {
                notify_guild_master(lc_text("이미 선전포고 중인 길드입니다."));
            }
            break;
        default:
            metin2::log::error("Unknown guild war state {:d} for guild {:d} vs {:d}, type {:d}, guild {} ({:d})",
                it->second.state, id(), opp_gid, type, name(), id());
            break;
        }
    }

    auto guild::declare_war(uint32_t opp_gid, uint8_t type, uint8_t state) -> bool {
        if (enemy_guild_.find(opp_gid) != enemy_guild_.end())
            return false;

        guild_war gw(type);
        gw.state = state;

        enemy_guild_.insert(std::make_pair(opp_gid, gw));

        guild_war_packet(opp_gid, type, state);
        return true;
    }

    auto guild::check_start_war(uint32_t opp_gid) -> bool {
        auto it = enemy_guild_.find(opp_gid);

        if (it == enemy_guild_.end())
            return false;

        guild_war &gw(it->second);

        if (gw.state == GUILD_WAR_ON_WAR)
            return false;

        return true;
    }

    void guild::start_war(uint32_t opp_gid) {
        auto it = enemy_guild_.find(opp_gid);

        if (it == enemy_guild_.end())
            return;

        guild_war &gw(it->second);

        if (gw.state == GUILD_WAR_ON_WAR)
            return;

        gw.state = GUILD_WAR_ON_WAR;
        gw.war_start_time = get_global_time();

        guild_war_packet(opp_gid, gw.type, GUILD_WAR_ON_WAR);

        if (gw.type != GUILD_WAR_TYPE_FIELD)
            guild_war_entry_ask(opp_gid);
    }

    auto guild::wait_start_war(uint32_t opp_gid) -> bool {
        if (opp_gid == id()) {
            metin2::log::warn("Cannot start war, guild {:d} is at war with itself ({:d})", id(), opp_gid);
            return false;
        }

        auto it = enemy_guild_.find(opp_gid);
        if (it == enemy_guild_.end()) {
            metin2::log::warn("Cannot start war, guild {:d} has no war entry for guild {:d}", id(), opp_gid);
            return false;
        }

        guild_war &gw(it->second);

        if (gw.state == GUILD_WAR_WAIT_START) {
            metin2::log::warn("Cannot start war, guild {:d} vs {:d} is already waiting to start (state {:d})", id(), opp_gid, gw.state);
            return false;
        }

        gw.state = GUILD_WAR_WAIT_START;

        guild *g = guild_manager::instance().find_guild(opp_gid);
        if (!g) {
            metin2::log::warn("Cannot start war for guild {:d}, enemy guild {:d} not found", id(), opp_gid);
            return false;
        }

        const guild_war_info &guild_war_info = guild_war_get_type_info(gw.type);

        if (gw.type == GUILD_WAR_TYPE_FIELD) {
            metin2::log::info("Guild war: guild {:d} vs {:d} is a field war, no war map needed", id(), opp_gid);
            return true;
        }

        metin2::log::debug("Guild war: checking whether this server hosts the war map for guild {:d} vs {:d}, type {:d}, map {:d}",
            id(), opp_gid, gw.type, guild_war_info.map_index);

        if (!map_allow_find(guild_war_info.map_index)) {
            metin2::log::debug("Guild war: war map for guild {:d} vs {:d} is hosted on another server", id(), opp_gid);
            return true;
        }

        uint32_t id1 = id();
        uint32_t id2 = opp_gid;

        if (id1 > id2)
            std::swap(id1, id2);

        uint32_t map_index = war_map_manager::instance().create_war_map(guild_war_info, id1, id2);
        if (!map_index) {
            metin2::log::error("Failed to create war map for guild {:d} vs {:d}, type {:d}, map {:d}, ending the war", id1, id2, gw.type, guild_war_info.map_index);
            guild_manager::instance().request_end_war(id(), opp_gid);
            return false;
        }

        metin2::log::info("Guild war: created war map for guild {:d} vs {:d}, type {:d}, map {:d}, instance {:d}", id1, id2, gw.type, guild_war_info.map_index, map_index);

        gw.map_index = map_index;

        set_guild_war_map_index(opp_gid, map_index);
        g->set_guild_war_map_index(id(), map_index);

        packet_gg_guild_war_map_index p;

        p.header = HEADER_GG_GUILD_WAR_ZONE_MAP_INDEX;
        p.guild_id1 = id1;
        p.guild_id2 = id2;
        p.map_index = map_index;

        p2p_manager::instance().send(&p, sizeof(p));

        return true;
    }

    void guild::request_refuse_war(uint32_t opp_gid) {
        if (opp_gid == id())
            return;

        auto it = enemy_guild_.find(opp_gid);

        if (it != enemy_guild_.end() && it->second.state == GUILD_WAR_RECV_DECLARE) {
            packet_guild_war p;
            p.war = GUILD_WAR_REFUSE;
            p.guild_from = id();
            p.guild_to = opp_gid;

            db_clientdesc->db_packet(HEADER_GD_GUILD_WAR, 0, &p, sizeof(p));
        }
    }

    void guild::refuse_war(uint32_t opp_gid) {
        if (opp_gid == id())
            return;

        auto it = enemy_guild_.find(opp_gid);

        if (it != enemy_guild_.end() && (it->second.state == GUILD_WAR_SEND_DECLARE || it->second.state == GUILD_WAR_RECV_DECLARE)) {
            uint8_t type = it->second.type;
            enemy_guild_.erase(opp_gid);

            guild_war_packet(opp_gid, type, GUILD_WAR_END);
        }
    }

    void guild::reserve_war(uint32_t opp_gid, uint8_t type) {
        if (opp_gid == id())
            return;

        auto it = enemy_guild_.find(opp_gid);

        if (it == enemy_guild_.end()) {
            guild_war gw(type);
            gw.state = GUILD_WAR_RESERVE;
            enemy_guild_.insert(std::make_pair(opp_gid, gw));
        } else
            it->second.state = GUILD_WAR_RESERVE;

        metin2::log::info("Guild war: guild {:d} reserved war against guild {:d}", id(), opp_gid);
    }

    void guild::end_war(uint32_t opp_gid) {
        if (opp_gid == id())
            return;

        auto it = enemy_guild_.find(opp_gid);

        if (it != enemy_guild_.end()) {
            war_map *map = war_map_manager::instance().find(it->second.map_index);

            if (map)
                map->set_ended();

            guild_war_packet(opp_gid, it->second.type, GUILD_WAR_END);
            enemy_guild_.erase(it);

            if (!under_any_war()) {
                for (auto it = member_online_.begin(); it != member_online_.end(); ++it) {
                    character *ch = *it;
                    ch->remove_affect(GUILD_SKILL_BLOOD);
                    ch->remove_affect(GUILD_SKILL_BLESS);
                    ch->remove_affect(GUILD_SKILL_SEONGHWI);
                    ch->remove_affect(GUILD_SKILL_ACCEL);
                    ch->remove_affect(GUILD_SKILL_BUNNO);
                    ch->remove_affect(GUILD_SKILL_JUMUN);

                    ch->remove_bad_affect();
                }
            }
        }
    }

    void guild::set_guild_war_map_index(uint32_t opp_gid, int32_t map_index_value) {
        auto it = enemy_guild_.find(opp_gid);

        if (it == enemy_guild_.end())
            return;

        it->second.map_index = map_index_value;
        metin2::log::info("Guild war: guild {:d} vs {:d} war map set to {:d}", id(), opp_gid, map_index_value);
    }

    void guild::guild_war_entry_accept(uint32_t opp_gid, character *ch) {
        auto git = enemy_guild_.find(opp_gid);

        if (git == enemy_guild_.end())
            return;

        guild_war &gw(git->second);

        if (gw.type == GUILD_WAR_TYPE_FIELD)
            return;

        if (gw.state != GUILD_WAR_ON_WAR) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("이미 전쟁이 끝났습니다."));
            return;
        }

        if (!gw.map_index)
            return;

        pixel_position pos;

        if (!war_map_manager::instance().start_position(gw.map_index, id() < opp_gid ? 0 : 1, pos))
            return;

        quest::pc *pc = quest::quest_manager::instance().get_pc(ch->player_id());
        pc->set_flag("war.is_war_member", 1);

        ch->save_exit_location();
        ch->warp_set(pos.x, pos.y, gw.map_index);
    }

    void guild::guild_war_entry_ask(uint32_t opp_gid) {
        auto git = enemy_guild_.find(opp_gid);
        if (git == enemy_guild_.end()) {
            metin2::log::error("Cannot send war entry request, guild {:d} has no war entry for guild {:d}", id(), opp_gid);
            return;
        }

        guild_war &gw(git->second);

        metin2::log::info("Guild war: asking members of guild {:d} to join the war against guild {:d}, map {:d}", id(), opp_gid, gw.map_index);
        if (!gw.map_index) {
            metin2::log::error("Cannot send war entry request for guild {:d} vs {:d}, no war map", id(), opp_gid);
            return;
        }

        pixel_position pos;
        if (!war_map_manager::instance().start_position(gw.map_index, id() < opp_gid ? 0 : 1, pos)) {
            metin2::log::error("Cannot send war entry request for guild {:d} vs {:d}, no start position ({:d}, {:d})", id(), opp_gid, pos.x, pos.y);
            return;
        }

        metin2::log::debug("Guild war: {:d} members online to ask", member_online_.size());

        auto it = member_online_.begin();

        while (it != member_online_.end()) {
            character *ch = *it++;

            using namespace quest;
            unsigned int quest_index = quest_manager::instance().quest_index_by_name("guild_war_join");
            if (quest_index) {
                metin2::log::debug("Guild war: sent join letter to pid {:d} (quest {:d})", ch->player_id(), quest_index);
                quest_manager::instance().letter(ch->player_id(), quest_index, 0);
            } else {
                metin2::log::error("Cannot send war join letter to pid {:d}, quest guild_war_join.quest not found (index {:d})",
                    ch->player_id(), quest_index);
                break;
            }
        }
    }

    void guild::set_ladder_point(int point) {
        if (data_.ladder_point != point) {
            char buf[256];
            snprintf(buf, sizeof(buf), lc_text("<길드> 래더 점수가 %d 점이 되었습니다"), point);
            for (auto it = member_online_.begin(); it != member_online_.end(); ++it) {
                character *ch = (*it);
                ch->chat_packet(CHAT_TYPE_INFO, buf);
            }
        }
        data_.ladder_point = point;
    }

    void guild::change_ladder_point(int change_value) {
        packet_guild_ladder_point p;
        p.guild = id();
        p.change = change_value;
        db_clientdesc->db_packet(HEADER_GD_GUILD_CHANGE_LADDER_POINT, 0, &p, sizeof(p));
    }
}
