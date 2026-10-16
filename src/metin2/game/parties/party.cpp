#include <metin2/config/config.h>
#include <metin2/game/parties/party.h>

#include <metin2/game/app/config.h>
#include <metin2/game/app/constants.h>
#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/items/unique_item.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/protocol/p2p.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/world/dungeon.h>
#include <metin2/game/world/sectree_manager.h>

#include <metin2/core/utils.h>
#include <metin2/log/log.h>
#include <metin2/proto/character_types.h>
#include <metin2/proto/chat_types.h>
#include <metin2/proto/limits.h>
#include <metin2/proto/tables.h>

#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>

namespace metin2::game::inline parties {

    namespace {
        template <class P>
        void send_to_online(party::member_map &members, const P &p) {
            for (auto &[pid, m] : members)
                if (m.character && m.character->get_desc())
                    m.character->get_desc()->packet(&p, sizeof(p));
        }

        auto offline_update_packet(uint32_t pid, uint8_t role) -> packet_gc_party_update {
            packet_gc_party_update p;
            memset(&p, 0, sizeof(p));
            p.header = HEADER_GC_PARTY_UPDATE;
            p.pid = pid;
            p.percent_hp = 255;
            p.role = role;
            return p;
        }
    }

    party_manager::party_manager() {
        initialize();
    }

    party_manager::~party_manager() = default;

    void party_manager::initialize() {
        enable_pc_party_ = false;
    }

    void party_manager::delete_all_party() {
        auto it = set_pc_party_.begin();

        while (it != set_pc_party_.end()) {
            delete_party(*it);
            it = set_pc_party_.begin();
        }
    }

    auto party_manager::set_party(character *ch) -> bool {
        auto it = map_party_.find(ch->player_id());

        if (it == map_party_.end())
            return false;

        party *party = it->second;
        party->link(ch);
        return true;
    }

    void party_manager::p2p_login(uint32_t pid, const char *name) {
        auto it = map_party_.find(pid);

        if (it == map_party_.end())
            return;

        it->second->update_online_state(pid, name);
    }
    void party_manager::p2p_logout(uint32_t pid) {
        auto it = map_party_.find(pid);

        if (it == map_party_.end())
            return;

        it->second->update_offline_state(pid);
    }

    void party_manager::p2p_join_party(uint32_t leader, uint32_t pid, uint8_t role) {
        auto it = map_party_.find(leader);

        if (it != map_party_.end()) {
            it->second->p2p_join(pid);

            if (role >= PARTY_ROLE_MAX_NUM)
                role = PARTY_ROLE_NORMAL;

            it->second->set_role(pid, role, true);
        } else {
            metin2::log::error("No party with leader pid {:d}", leader);
        }
    }

    void party_manager::p2p_quit_party(uint32_t pid) {
        auto it = map_party_.find(pid);

        if (it != map_party_.end()) {
            it->second->p2p_quit(pid);
        } else {
            metin2::log::error("No party with member pid {:d}", pid);
        }
    }

    auto party_manager::p2p_create_party(uint32_t pid) -> party * {
        auto it = map_party_.find(pid);
        if (it != map_party_.end())
            return it->second;

        auto *party_value = new party;

        set_pc_party_.insert(party_value);

        set_party_member(pid, party_value);
        party_value->set_pc_party(true);
        party_value->p2p_join(pid);

        return party_value;
    }

    void party_manager::p2p_delete_party(uint32_t pid) {
        auto it = map_party_.find(pid);

        if (it != map_party_.end()) {
            set_pc_party_.erase(it->second);
            delete it->second;
        } else
            metin2::log::error("Cannot delete party of leader pid {:d}: not found", pid);
    }

    auto party_manager::create_party(character *leader) -> party * {
        if (leader->get_party())
            return leader->get_party();

        auto *party_value = new party;

        if (leader->is_pc()) {
            packet_party_create p;
            p.leader_pid = leader->player_id();

            db_clientdesc->db_packet(HEADER_GD_PARTY_CREATE, 0, &p, sizeof(packet_party_create));

            metin2::log::info("Party: created by {} (pid {:d})", leader->name(), leader->player_id());
            party_value->set_pc_party(true);
            party_value->join(leader->player_id());

            set_pc_party_.insert(party_value);
        } else {
            party_value->set_pc_party(false);
            party_value->join(leader->get_vid());
        }

        party_value->link(leader);
        return (party_value);
    }

    void party_manager::delete_party(party *party_value) {
        packet_party_delete p;
        p.leader_pid = party_value->leader_pid();

        db_clientdesc->db_packet(HEADER_GD_PARTY_DELETE, 0, &p, sizeof(packet_party_delete));

        set_pc_party_.erase(party_value);
        delete party_value;
    }

    void party_manager::set_party_member(uint32_t pid, party *party_value) {
        auto it = map_party_.find(pid);

        if (party_value == nullptr) {
            if (it != map_party_.end())
                map_party_.erase(it);
        } else {
            if (it != map_party_.end()) {
                if (it->second != party_value) {
                    it->second->quit(pid);
                    it->second = party_value;
                }
            } else
                map_party_.insert(party_map::value_type(pid, party_value));
        }
    }

    EVENTINFO(party_update_event_info) {
        uint32_t pid;

        party_update_event_info()
            : pid(0) {
        }
    };

    EVENTFUNC(party_update_event) {
        auto *info = dynamic_cast<party_update_event_info *>(event->info.get());

        if (info == nullptr) {
            metin2::log::error("Party update event fired without event info");
            return 0;
        }

        uint32_t pid = info->pid;
        character *leader = get_character_manager().find_by_pid(pid);

        if (leader && leader->get_desc()) {
            party *party = leader->get_party();

            if (party)
                party->update();
        }

        return passes_per_sec(3);
    }

    party::party() {
        initialize();
    }

    party::~party() {
        destroy();
    }

    void party::initialize() {

        exp_distribution_mode_ = PARTY_EXP_DISTRIBUTION_NON_PARITY;
        chr_exp_centralize_ = nullptr;

        leader_pid_ = 0;

        event_update_ = nullptr;

        memset(&an_role_count_, 0, sizeof(an_role_count_));
        memset(&an_max_role_, 0, sizeof(an_max_role_));
        an_max_role_[PARTY_ROLE_LEADER] = 1;
        an_max_role_[PARTY_ROLE_NORMAL] = 32;

        party_start_time_ = get_dword_time();
        long_time_exp_bonus_ = 0;

        party_heal_time_ = get_dword_time();
        party_heal_ready_ = false;
        can_use_party_heal_ = false;

        leadership_ = 0;
        exp_bonus_ = 0;

        next_owner_ = member_map_.begin();

        count_near_party_member_ = 0;

        chr_leader_ = nullptr;
        pc_party_ = false;
        dungeon_ = nullptr;
        dungeon_for_only_party_ = nullptr;
    }

    void party::destroy() {

        if (pc_party_) {
            for (auto it = member_map_.begin(); it != member_map_.end(); ++it)
                get_party_manager().set_party_member(it->first, nullptr);
        }

        event_cancel(&event_update_);

        remove_bonus();

        auto it = member_map_.begin();

        uint32_t time = get_dword_time();

        while (it != member_map_.end()) {
            member &member = it->second;
            ++it;

            if (member.character) {
                if (member.character->get_desc()) {
                    packet_gc_party_remove p;
                    p.header = HEADER_GC_PARTY_REMOVE;
                    p.pid = member.character->player_id();
                    member.character->get_desc()->packet(&p, sizeof(p));
                    member.character->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 파티가 해산 되었습니다."));
                } else {
                    member.character->set_last_attacked(time);
                    member.character->start_destroy_when_idle_event();
                }

                member.character->set_party(nullptr);
            }
        }

        member_map_.clear();
        next_owner_ = member_map_.begin();

        if (dungeon_for_only_party_ != nullptr) {
            dungeon_for_only_party_->set_party_null();
            dungeon_for_only_party_ = nullptr;
        }
    }

    void party::chat_packet_to_all_member(uint8_t type, const char *format, ...) {
        char chatbuf[CHAT_MAX_LEN + 1];
        va_list args;

        va_start(args, format);
        vsnprintf(chatbuf, sizeof(chatbuf), format, args);
        va_end(args);

        member_map::iterator it;

        for (it = member_map_.begin(); it != member_map_.end(); ++it) {
            member &member = it->second;

            if (member.character) {
                if (member.character->get_desc()) {
                    member.character->chat_packet(type, "%s", chatbuf);
                }
            }
        }
    }

    auto party::leader_pid() -> uint32_t {
        return leader_pid_;
    }

    auto party::member_count() -> uint32_t {
        return member_map_.size();
    }

    void party::p2p_join(uint32_t pid) {
        auto it = member_map_.find(pid);

        if (it == member_map_.end()) {
            member member_value;

            member_value.character = nullptr;
            member_value.near = false;

            if (member_map_.empty()) {
                member_value.role = PARTY_ROLE_LEADER;
                leader_pid_ = pid;
            } else
                member_value.role = PARTY_ROLE_NORMAL;

            if (pc_party_) {
                character *ch = get_character_manager().find_by_pid(pid);

                if (ch) {
                    metin2::log::info("Party: {} (pid {:d}) joined the party of leader pid {:d}", ch->name(), pid, leader_pid_);
                    member_value.name = ch->name();

                    if (member_value.role == PARTY_ROLE_LEADER)
                        leadership_ = ch->leadership_skill_level();
                } else {
                    cci *pcci = get_p2p_manager().find_by_pid(pid);

                    if (!pcci)
                        ;
                    else if (pcci->channel == metin2::config::server().channel)
                        member_value.name = pcci->name;
                    else
                        metin2::log::error("Party member pid {:d} is on channel {:d}, not this channel {:d}", pid, pcci->channel, metin2::config::server().channel);
                }
            }

            metin2::log::debug("Party of leader pid {:d}: member count {:d} -> {:d}", leader_pid(), member_count(), member_count() + 1);

            member_map_.insert(member_map::value_type(pid, member_value));

            if (member_map_.size() == 1)
                next_owner_ = member_map_.begin();

            if (pc_party_) {
                get_party_manager().set_party_member(pid, this);
                send_party_join_one_to_all(pid);

                character *ch = get_character_manager().find_by_pid(pid);

                if (ch)
                    send_parameter(ch);
            }
        }

        if (dungeon_) {
            dungeon_->quit_party(this);
        }
    }

    void party::join(uint32_t pid_value) {
        p2p_join(pid_value);

        if (pc_party_) {
            packet_party_add p;
            p.leader_pid = leader_pid();
            p.pid = pid_value;
            p.state = PARTY_ROLE_NORMAL;
            db_clientdesc->db_packet(HEADER_GD_PARTY_ADD, 0, &p, sizeof(p));
        }
    }

    void party::p2p_quit(uint32_t pid) {
        auto it = member_map_.find(pid);

        if (it == member_map_.end())
            return;

        if (pc_party_)
            send_party_remove_one_to_all(pid);

        if (it == next_owner_)
            increase_ownership();

        if (pc_party_)
            remove_bonus_for_one(pid);

        character *ch = it->second.character;
        uint8_t role_value = it->second.role;

        member_map_.erase(it);

        metin2::log::debug("Party of leader pid {:d}: member count {:d} -> {:d}", leader_pid(), member_count(), member_count() - 1);

        if (role_value < PARTY_ROLE_MAX_NUM) {
            --an_role_count_[role_value];
        } else {
            metin2::log::error("Party role {:d} out of range (max {:d}) on member quit", role_value, PARTY_ROLE_MAX_NUM);
        }

        if (ch) {
            ch->set_party(nullptr);
            compute_role_point(ch, role_value, false);
        }

        if (pc_party_)
            get_party_manager().set_party_member(pid, nullptr);

        if (role_value == PARTY_ROLE_LEADER)
            get_party_manager().delete_party(this);

        // The party is deleted above; nothing may follow.
    }

    void party::quit(uint32_t pid_value) {
        p2p_quit(pid_value);

        if (pc_party_ && pid_value != leader_pid()) {
            packet_party_remove p;
            p.pid = pid_value;
            p.leader_pid = leader_pid();
            db_clientdesc->db_packet(HEADER_GD_PARTY_REMOVE, 0, &p, sizeof(p));
        }
    }

    void party::link(character *chr) {
        member_map::iterator it;

        if (chr->is_pc())
            it = member_map_.find(chr->player_id());
        else
            it = member_map_.find(chr->get_vid());

        if (it == member_map_.end()) {
            metin2::log::error("{} is not a member of this party", chr->name());
            return;
        }

        if (pc_party_ && !event_update_) {
            auto *info = alloc_event_info<party_update_event_info>();
            info->pid = leader_pid_;
            event_update_ = event_create(party_update_event, info, passes_per_sec(3));
        }

        if (it->second.role == PARTY_ROLE_LEADER)
            chr_leader_ = chr;

        metin2::log::debug("Party of leader pid {:d}: {} linked", leader_pid(), chr->name());

        it->second.character = chr;
        chr->set_party(this);

        if (chr->is_pc()) {
            if (it->second.name.empty()) {
                it->second.name = chr->name();
            }

            send_party_join_one_to_all(chr->player_id());

            send_party_join_all_to_one(chr);
            send_party_link_one_to_all(chr);
            send_party_link_all_to_one(chr);
            send_party_info_all_to_one(chr);
            send_party_info_one_to_all(chr);

            send_parameter(chr);

            if (get_dungeon() && get_dungeon()->map_index() == chr->map_index()) {
                chr->set_dungeon(get_dungeon());
            }

            request_set_member_level(chr->player_id(), chr->level());
        }
    }

    void party::request_set_member_level(uint32_t pid, uint8_t level) {
        packet_party_set_member_level p;
        p.leader_pid = leader_pid();
        p.pid = pid;
        p.level = level;
        db_clientdesc->db_packet(HEADER_GD_PARTY_SET_MEMBER_LEVEL, 0, &p, sizeof(packet_party_set_member_level));
    }

    void party::p2p_set_member_level(uint32_t pid, uint8_t level) {
        if (!pc_party_)
            return;

        member_map::iterator it;

        metin2::log::debug("Party of leader pid {:d}: member pid {:d} level set to {:d}", leader_pid(), pid, level);

        it = member_map_.find(pid);
        if (it != member_map_.end()) {
            it->second.level = level;
        }
    }

    void party::unlink(character *chr) {
        member_map::iterator it;

        if (chr->is_pc())
            it = member_map_.find(chr->player_id());
        else
            it = member_map_.find(chr->get_vid());

        if (it == member_map_.end()) {
            metin2::log::error("{} is not a member of this party", chr->name());
            return;
        }

        if (chr->is_pc()) {
            send_party_unlink_one_to_all(chr);

            if (it->second.role == PARTY_ROLE_LEADER) {
                remove_bonus();

                if (it->second.character->get_dungeon()) {
                    for_each_near_member([](character *ch) { ch->exit_to_saved_location(); });
                }
            }
        }

        if (it->second.role == PARTY_ROLE_LEADER)
            chr_leader_ = nullptr;

        it->second.character = nullptr;
        chr->set_party(nullptr);
    }

    void party::send_party_remove_one_to_all(uint32_t pid) {
        packet_gc_party_remove p;
        p.header = HEADER_GC_PARTY_REMOVE;
        p.pid = pid;

        send_to_online(member_map_, p);
    }

    void party::send_party_join_one_to_all(uint32_t pid) {
        auto found = member_map_.find(pid);
        if (found == member_map_.end())
            return;
        const member &r = found->second;

        packet_gc_party_add p;

        p.header = HEADER_GC_PARTY_ADD;
        p.pid = pid;
        strlcpy(p.name, r.name.c_str(), sizeof(p.name));

        send_to_online(member_map_, p);
    }

    void party::send_party_join_all_to_one(character *ch) {
        if (!ch->get_desc())
            return;

        packet_gc_party_add p;

        p.header = HEADER_GC_PARTY_ADD;
        p.name[CHARACTER_NAME_MAX_LEN] = '\0';

        for (auto it = member_map_.begin(); it != member_map_.end(); ++it) {
            p.pid = it->first;
            strlcpy(p.name, it->second.name.c_str(), sizeof(p.name));
            ch->get_desc()->packet(&p, sizeof(p));
        }
    }

    void party::send_party_unlink_one_to_all(character *ch) {
        if (!ch->get_desc())
            return;

        packet_gc_party_link p;
        p.header = HEADER_GC_PARTY_UNLINK;
        p.pid = ch->player_id();
        p.vid = (uint32_t)ch->get_vid();

        send_to_online(member_map_, p);
    }

    void party::send_party_link_one_to_all(character *ch) {
        if (!ch->get_desc())
            return;

        packet_gc_party_link p;
        p.header = HEADER_GC_PARTY_LINK;
        p.vid = ch->get_vid();
        p.pid = ch->player_id();

        send_to_online(member_map_, p);
    }

    void party::send_party_link_all_to_one(character *ch) {
        if (!ch->get_desc())
            return;

        member_map::iterator it;

        packet_gc_party_link p;
        p.header = HEADER_GC_PARTY_LINK;

        for (it = member_map_.begin(); it != member_map_.end(); ++it) {
            if (it->second.character) {
                p.vid = it->second.character->get_vid();
                p.pid = it->second.character->player_id();
                ch->get_desc()->packet(&p, sizeof(p));
            }
        }
    }

    void party::send_party_info_one_to_all(uint32_t pid) {
        auto it = member_map_.find(pid);

        if (it == member_map_.end())
            return;

        if (it->second.character) {
            send_party_info_one_to_all(it->second.character);
            return;
        }

        send_to_online(member_map_, offline_update_packet(pid, it->second.role));
    }

    void party::send_party_info_one_to_all(character *ch) {
        if (!ch->get_desc())
            return;

        member_map::iterator it;

        packet_gc_party_update p;
        ch->build_update_party_packet(p);

        for (it = member_map_.begin(); it != member_map_.end(); ++it) {
            if ((it->second.character) && (it->second.character->get_desc())) {
                metin2::log::debug("Party: sending info of {} (vid {:d}) to {} (vid {:d})", ch->name(), (uint32_t)ch->get_vid(), it->second.character->name(), (uint32_t)it->second.character->get_vid());
                it->second.character->get_desc()->packet(&p, sizeof(p));
            }
        }
    }

    void party::send_party_info_all_to_one(character *ch) {
        if (!ch->get_desc())
            return;

        member_map::iterator it;

        packet_gc_party_update p;

        for (it = member_map_.begin(); it != member_map_.end(); ++it) {
            if (!it->second.character) {
                p = offline_update_packet(it->first, it->second.role);
                ch->get_desc()->packet(&p, sizeof(p));
                continue;
            }

            it->second.character->build_update_party_packet(p);
            metin2::log::debug("Party: sending info of {} (vid {:d}) to {} (vid {:d})", it->second.character->name(), (uint32_t)it->second.character->get_vid(), ch->name(), (uint32_t)ch->get_vid());
            ch->get_desc()->packet(&p, sizeof(p));
        }
    }

    void party::send_message(character *ch, uint8_t msg, uint32_t arg1, uint32_t arg2) {
        if (ch->get_party() != this) {
            metin2::log::error("{} is not a member of party {}", ch->name(), static_cast<const void *>(this));
            return;
        }

        auto for_each_other_member = [&](auto &&f) {
            auto it = member_map_.begin();

            while (it != member_map_.end()) {
                character *chr = it->second.character;
                ++it;

                if (chr && ch != chr)
                    f(chr);
            }
        };

        switch (msg) {
        case PM_ATTACK:
            break;

        case PM_RETURN:
            for_each_other_member([&](character *member) {
                uint32_t x = arg1 + number(-500, 500);
                uint32_t y = arg2 + number(-500, 500);

                member->set_victim(nullptr);
                member->set_rotation_to_xy(x, y);

                if (member->go_to(x, y)) {
                    character *victim = member->victim();
                    metin2::log::debug("Party: member {} ({}) returns to the leader, victim {}", member->name(), static_cast<const void *>(member), static_cast<const void *>(victim));
                    member->send_move_packet(FUNC_WAIT, 0, 0, 0, 0);
                }
            });
            break;

        case PM_ATTACKED_BY:
            {
                character *chr_victim = ch->victim();

                if (!chr_victim)
                    return;

                for_each_other_member([&](character *member) {
                    if (member->can_begin_fight())
                        member->begin_fight(chr_victim);
                });
            }
            break;

        case PM_AGGRO_INCREASE:
            {
                character *victim = get_character_manager().find(arg2);

                if (!victim)
                    return;

                for_each_other_member([&](character *member) { member->update_aggr_point(victim, DAMAGE_TYPE_SPECIAL, arg1); });
            }
            break;
        }
    }

    auto party::leader_character() -> character * {
        auto it = member_map_.find(leader_pid());
        return it == member_map_.end() ? nullptr : it->second.character;
    }

    auto party::set_role(uint32_t pid, uint8_t role_value, bool set) -> bool {
        auto it = member_map_.find(pid);

        if (it == member_map_.end()) {
            return false;
        }

        character *ch = it->second.character;

        if (set) {
            if (an_role_count_[role_value] >= an_max_role_[role_value])
                return false;

            if (it->second.role != PARTY_ROLE_NORMAL)
                return false;

            it->second.role = role_value;

            if (ch && leader())
                compute_role_point(ch, role_value, true);

            if (role_value < PARTY_ROLE_MAX_NUM) {
                ++an_role_count_[role_value];
            } else {
                metin2::log::error("Party role {:d} out of range (max {:d}) on role count increment", role_value, PARTY_ROLE_MAX_NUM);
            }
        } else {
            if (it->second.role == PARTY_ROLE_LEADER)
                return false;

            if (it->second.role == PARTY_ROLE_NORMAL)
                return false;

            it->second.role = PARTY_ROLE_NORMAL;

            if (ch && leader())
                compute_role_point(ch, PARTY_ROLE_NORMAL, false);

            if (role_value < PARTY_ROLE_MAX_NUM) {
                --an_role_count_[role_value];
            } else {
                metin2::log::error("Party role {:d} out of range (max {:d}) on role count decrement", role_value, PARTY_ROLE_MAX_NUM);
            }
        }

        send_party_info_one_to_all(pid);
        return true;
    }

    auto party::role(uint32_t pid) -> uint8_t {
        auto it = member_map_.find(pid);

        if (it == member_map_.end())
            return PARTY_ROLE_NORMAL;
        else
            return it->second.role;
    }

    void party::remove_bonus() {
        member_map::iterator it;

        for (it = member_map_.begin(); it != member_map_.end(); ++it) {
            character *ch;

            if ((ch = it->second.character)) {
                compute_role_point(ch, it->second.role, false);
            }

            it->second.near = false;
        }
    }

    void party::remove_bonus_for_one(uint32_t pid) {
        auto it = member_map_.find(pid);

        if (it == member_map_.end())
            return;

        character *ch;

        if ((ch = it->second.character))
            compute_role_point(ch, it->second.role, false);
    }

    void party::heal_party() {
        {
            return;
        }
        if (!party_heal_ready_)
            return;

        member_map::iterator it;
        character *l = leader_character();

        for (it = member_map_.begin(); it != member_map_.end(); ++it) {
            if (!it->second.character)
                continue;

            character *ch = it->second.character;

            if (distance_approx(l->x() - ch->x(), l->y() - ch->y()) < PARTY_DEFAULT_RANGE) {
                ch->point_change(POINT_HP, ch->max_hp() - ch->hp());
                ch->point_change(POINT_SP, ch->max_sp() - ch->sp());
            }
        }

        party_heal_ready_ = false;
        party_heal_time_ = get_dword_time();
    }

    void party::summon_to_leader(uint32_t pid) {
        int xy[12][2] =
            {
                {250,  0   },
                {216,  125 },
                {125,  216 },
                {0,    250 },
                {-125, 216 },
                {-216, 125 },
                {-250, 0   },
                {-216, -125},
                {-125, -216},
                {0,    -250},
                {125,  -216},
                {216,  -125},
        };

        int n = 0;
        int x[12], y[12];

        sectree_manager &s = get_sectree_manager();
        character *l = leader_character();

        if (member_map_.find(pid) == member_map_.end()) {
            l->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 소환하려는 대상을 찾을 수 없습니다."));
            return;
        }

        auto found = member_map_.find(pid);
        character *ch = found == member_map_.end() ? nullptr : found->second.character;

        if (!ch) {
            l->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 소환하려는 대상을 찾을 수 없습니다."));
            return;
        }

        if (!ch->can_summon(leadership_)) {
            l->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 대상을 소환할 수 없습니다."));
            return;
        }

        for (int i = 0; i < 12; ++i) {
            pixel_position p;

            if (s.movable_position(l->map_index(), l->x() + xy[i][0], l->y() + xy[i][1], p)) {
                x[n] = p.x;
                y[n] = p.y;
                n++;
            }
        }

        if (n == 0)
            l->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 파티원을 현재 위치로 소환할 수 없습니다."));
        else {
            int i = number(0, n - 1);
            ch->show(l->map_index(), x[i], y[i]);
            ch->stop();
        }
    }

    void party::increase_ownership() {
        if (member_map_.empty()) {
            next_owner_ = member_map_.begin();
            return;
        }

        if (next_owner_ == member_map_.end())
            next_owner_ = member_map_.begin();
        else {
            next_owner_++;

            if (next_owner_ == member_map_.end())
                next_owner_ = member_map_.begin();
        }
    }

    auto party::next_ownership(character *ch, int32_t x, int32_t y) -> character * {
        if (next_owner_ == member_map_.end())
            return ch;

        int size = member_map_.size();

        while (size-- > 0) {
            character *member = next_owner_->second.character;

            if (member && distance_approx(member->x() - x, member->y() - y) < 3000) {
                increase_ownership();
                return member;
            }

            increase_ownership();
        }

        return ch;
    }

    void party::compute_role_point(character *ch, uint8_t role, bool add) {
        if (!add) {
            ch->point_change(POINT_PARTY_ATTACKER_BONUS, -ch->point(POINT_PARTY_ATTACKER_BONUS));
            ch->point_change(POINT_PARTY_TANKER_BONUS, -ch->point(POINT_PARTY_TANKER_BONUS));
            ch->point_change(POINT_PARTY_BUFFER_BONUS, -ch->point(POINT_PARTY_BUFFER_BONUS));
            ch->point_change(POINT_PARTY_SKILL_MASTER_BONUS, -ch->point(POINT_PARTY_SKILL_MASTER_BONUS));
            ch->point_change(POINT_PARTY_DEFENDER_BONUS, -ch->point(POINT_PARTY_DEFENDER_BONUS));
            ch->point_change(POINT_PARTY_HASTE_BONUS, -ch->point(POINT_PARTY_HASTE_BONUS));
            ch->compute_battle_points();
            return;
        }

        float k = (float)ch->skill_power_by_level(std::min(SKILL_MAX_LEVEL, leadership_)) / 100.0f;

        switch (role) {
        case PARTY_ROLE_ATTACKER:
            {
                int bonus = (int)(10 + 60 * k);

                if (ch->point(POINT_PARTY_ATTACKER_BONUS) != bonus) {
                    ch->point_change(POINT_PARTY_ATTACKER_BONUS, bonus - ch->point(POINT_PARTY_ATTACKER_BONUS));
                    ch->compute_points();
                }
            }
            break;

        case PARTY_ROLE_TANKER:
            {
                int bonus = (int)(50 + 1450 * k);

                if (ch->point(POINT_PARTY_TANKER_BONUS) != bonus) {
                    ch->point_change(POINT_PARTY_TANKER_BONUS, bonus - ch->point(POINT_PARTY_TANKER_BONUS));
                    ch->compute_points();
                }
            }
            break;

        case PARTY_ROLE_BUFFER:
            {
                int bonus = (int)(5 + 45 * k);

                if (ch->point(POINT_PARTY_BUFFER_BONUS) != bonus) {
                    ch->point_change(POINT_PARTY_BUFFER_BONUS, bonus - ch->point(POINT_PARTY_BUFFER_BONUS));
                }
            }
            break;

        case PARTY_ROLE_SKILL_MASTER:
            {
                int bonus = (int)(25 + 600 * k);

                if (ch->point(POINT_PARTY_SKILL_MASTER_BONUS) != bonus) {
                    ch->point_change(POINT_PARTY_SKILL_MASTER_BONUS, bonus - ch->point(POINT_PARTY_SKILL_MASTER_BONUS));
                    ch->compute_points();
                }
            }
            break;
        case PARTY_ROLE_HASTE:
            {
                int bonus = (int)(1 + 5 * k);
                if (ch->point(POINT_PARTY_HASTE_BONUS) != bonus) {
                    ch->point_change(POINT_PARTY_HASTE_BONUS, bonus - ch->point(POINT_PARTY_HASTE_BONUS));
                    ch->compute_points();
                }
            }
            break;
        case PARTY_ROLE_DEFENDER:
            {
                int bonus = (int)(5 + 30 * k);
                if (ch->point(POINT_PARTY_DEFENDER_BONUS) != bonus) {
                    ch->point_change(POINT_PARTY_DEFENDER_BONUS, bonus - ch->point(POINT_PARTY_DEFENDER_BONUS));
                    ch->compute_points();
                }
            }
            break;
        }
    }

    void party::update() {

        character *l = leader_character();

        if (!l)
            return;

        member_map::iterator it;

        int near_member = 0;
        bool resend_all = false;

        for (it = member_map_.begin(); it != member_map_.end(); ++it) {
            character *ch = it->second.character;

            it->second.near = false;

            if (!ch)
                continue;

            if (l->get_dungeon())
                it->second.near = l->get_dungeon() == ch->get_dungeon();
            else
                it->second.near = (distance_approx(l->x() - ch->x(), l->y() - ch->y()) < PARTY_DEFAULT_RANGE);

            if (it->second.near) {
                ++near_member;
            }
        }

        if (near_member <= 1 && !l->get_dungeon()) {
            for (it = member_map_.begin(); it != member_map_.end(); ++it)
                it->second.near = false;

            near_member = 0;
        }

        if (near_member != count_near_party_member_) {
            count_near_party_member_ = near_member;
            resend_all = true;
        }

        leadership_ = l->leadership_skill_level();
        int new_exp_bonus = compute_party_bonus_exp_percent();

        if (exp_bonus_ != new_exp_bonus) {
            resend_all = true;
            exp_bonus_ = new_exp_bonus;
        }

        bool long_time_exp_bonus_changed = false;

        if (!long_time_exp_bonus_ && (get_dword_time() - party_start_time_ > PARTY_ENOUGH_MINUTE_FOR_EXP_BONUS * 60 * 1000)) {
            long_time_exp_bonus_changed = true;
            long_time_exp_bonus_ = 5;
            resend_all = true;
        }

        for (it = member_map_.begin(); it != member_map_.end(); ++it) {
            character *ch = it->second.character;

            if (!ch)
                continue;

            if (long_time_exp_bonus_changed && ch->get_desc())
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("파티의 협동력이 높아져 지금부터 추가 경험치 보너스를 받습니다."));

            bool near = it->second.near;

            compute_role_point(ch, it->second.role, near);

            if (near) {
                if (!resend_all)
                    send_party_info_one_to_all(ch);
            }
        }

        an_max_role_[PARTY_ROLE_ATTACKER] = leadership_ >= 10 ? 1 : 0;
        an_max_role_[PARTY_ROLE_HASTE] = leadership_ >= 20 ? 1 : 0;
        an_max_role_[PARTY_ROLE_TANKER] = leadership_ >= 20 ? 1 : 0;
        an_max_role_[PARTY_ROLE_BUFFER] = leadership_ >= 25 ? 1 : 0;
        an_max_role_[PARTY_ROLE_SKILL_MASTER] = leadership_ >= 35 ? 1 : 0;
        an_max_role_[PARTY_ROLE_DEFENDER] = leadership_ >= 40 ? 1 : 0;
        an_max_role_[PARTY_ROLE_ATTACKER] += leadership_ >= 40 ? 1 : 0;

        if (!party_heal_ready_) {
            if (!can_use_party_heal_ && leadership_ >= 18)
                party_heal_time_ = get_dword_time();

            can_use_party_heal_ = leadership_ >= 18;

            uint32_t party_heal_cool_time = (leadership_ >= 40) ? PARTY_HEAL_COOLTIME_SHORT * 60 * 1000 : PARTY_HEAL_COOLTIME_LONG * 60 * 1000;

            if (can_use_party_heal_) {
                if (get_dword_time() > party_heal_time_ + party_heal_cool_time) {
                    party_heal_ready_ = true;
                }
            }
        }

        if (resend_all) {
            for (auto it = member_map_.begin(); it != member_map_.end(); ++it)
                if (it->second.character)
                    send_party_info_one_to_all(it->second.character);
        }
    }

    void party::update_online_state(uint32_t pid_value, const char *name) {
        auto found = member_map_.find(pid_value);
        if (found == member_map_.end())
            return;
        member &r = found->second;

        packet_gc_party_add p;

        p.header = HEADER_GC_PARTY_ADD;
        p.pid = pid_value;
        r.name = name;
        strlcpy(p.name, name, sizeof(p.name));

        send_to_online(member_map_, p);
    }
    void party::update_offline_state(uint32_t pid_value) {

        packet_gc_party_add p;
        p.header = HEADER_GC_PARTY_ADD;
        p.pid = pid_value;
        memset(p.name, 0, CHARACTER_NAME_MAX_LEN + 1);

        send_to_online(member_map_, p);
    }

    auto party::flag(const std::string &name) -> int {
        auto it = flags_.find(name);

        if (it != flags_.end()) {
            return it->second;
        }

        return 0;
    }

    void party::set_flag(const std::string &name, int value) {
        auto it = flags_.find(name);

        if (it == flags_.end()) {
            flags_.insert(make_pair(name, value));
        } else if (it->second != value) {
            it->second = value;
        }
    }

    void party::set_dungeon(dungeon *dungeon_value) {
        dungeon_ = dungeon_value;
        flags_.clear();
    }

    auto party::get_dungeon() -> dungeon * {
        return dungeon_;
    }

    void party::set_dungeon_for_only_party(dungeon *dungeon_value) {
        dungeon_for_only_party_ = dungeon_value;
    }

    auto party::dungeon_for_only_party() -> dungeon * {
        return dungeon_for_only_party_;
    }

    auto party::is_position_near_leader(character *ch) -> bool {
        if (!chr_leader_)
            return false;

        if (distance_approx(ch->x() - chr_leader_->x(), ch->y() - chr_leader_->y()) >= PARTY_DEFAULT_RANGE)
            return false;

        return true;
    }

    auto party::exp_bonus_percent() -> int {
        if (near_member_count() <= 1)
            return 0;

        return exp_bonus_ + long_time_exp_bonus_;
    }

    auto party::count_member_by_vnum(uint32_t vnum_value) -> uint8_t {
        if (pc_party_)
            return 0;

        character *tch;
        uint8_t count = 0;

        member_map::iterator it;

        for (it = member_map_.begin(); it != member_map_.end(); ++it) {
            if (!(tch = it->second.character))
                continue;

            if (tch->is_pc())
                continue;

            if (tch->get_mob_table().vnum == vnum_value)
                ++count;
        }

        return count;
    }

    void party::send_parameter(character *ch) {
        packet_gc_party_parameter p;

        p.header = HEADER_GC_PARTY_PARAMETER;
        p.distribute_mode = exp_distribution_mode_;

        desc *d = ch->get_desc();

        if (d) {
            d->packet(&p, sizeof(packet_gc_party_parameter));
        }
    }

    void party::send_parameter_to_all() {
        if (!pc_party_)
            return;

        member_map::iterator it;

        for (it = member_map_.begin(); it != member_map_.end(); ++it)
            if (it->second.character)
                send_parameter(it->second.character);
    }

    void party::set_parameter(int mode) {
        if (mode >= PARTY_EXP_DISTRIBUTION_MAX_NUM) {
            metin2::log::error("Invalid party exp distribution mode {:d}", mode);
            return;
        }

        exp_distribution_mode_ = mode;
        send_parameter_to_all();
    }

    auto party::exp_distribution_mode() -> int {
        return exp_distribution_mode_;
    }

    auto party::exp_centralize_character() -> character * {
        return chr_exp_centralize_;
    }

    auto party::member_max_level() -> uint8_t {
        uint8_t max = 0;

        auto it = member_map_.begin();
        while (it != member_map_.end()) {
            if (!it->second.level) {
                ++it;
                continue;
            }

            if (!max)
                max = it->second.level;
            else if (it->second.level)
                max = std::max<int>(max, it->second.level);
            ++it;
        }
        return max;
    }

    auto party::compute_party_bonus_exp_percent() -> int {
        if (near_member_count() <= 1)
            return 0;

        character *leader = leader_character();

        int bonus_party_exp_from_item = 0;

        int member_count = std::min<int>(8, near_member_count());

        if (leader && (leader->is_equip_unique_item(UNIQUE_ITEM_PARTY_BONUS_EXP) || leader->is_equip_unique_item(UNIQUE_ITEM_PARTY_BONUS_EXP_MALL) || leader->is_equip_unique_item(UNIQUE_ITEM_PARTY_BONUS_EXP_GIFT) || leader->is_equip_unique_group(10010))) {
            bonus_party_exp_from_item = 30;
        }

        return bonus_party_exp_from_item + chn_ai_party_bonus_exp_percent_by_member_count[member_count];
    }
}
