#include <metin2/config/config.h>
#include <metin2/game/guilds/war_map.h>

#include <metin2/game/app/config.h>
#include <metin2/game/app/constants.h>
#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/characters/affect.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/guilds/guild_manager.h>
#include <metin2/game/items/item.h>
#include <metin2/game/protocol/buffer_manager.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/desc_manager.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/world/sectree_manager.h>

#include <metin2/core/utils.h>
#include <metin2/log/log.h>
#include <metin2/proto/chat_types.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <format>
#include <map>
#include <memory>
#include <string>
#include <utility>

namespace metin2::game::inline guilds {

    EVENTINFO(war_map_event_info) {
        int step;
        game::guilds::war_map *war_map;

        war_map_event_info()
            : step(0), war_map(nullptr) {
        }
    };

    EVENTFUNC(war_begin_event) {
        auto *info = dynamic_cast<war_map_event_info *>(event->info.get());

        if (info == nullptr) {
            metin2::log::error("Guild war begin event has no event info");
            return 0;
        }

        war_map *map = info->war_map;
        map->check_war_end();
        return passes_per_sec(10);
    }

    EVENTFUNC(war_end_event) {
        auto *info = dynamic_cast<war_map_event_info *>(event->info.get());

        if (info == nullptr) {
            metin2::log::error("Guild war end event has no event info");
            return 0;
        }

        war_map *map = info->war_map;

        if (info->step == 0) {
            ++info->step;
            map->exit_all();
            return passes_per_sec(5);
        } else {
            map->set_end_event(nullptr);
            get_war_map_manager().destroy_war_map(map);
            return 0;
        }
    }

    EVENTFUNC(war_timeout_event) {
        auto *info = dynamic_cast<war_map_event_info *>(event->info.get());

        if (info == nullptr) {
            metin2::log::error("Guild war timeout event has no event info");
            return 0;
        }

        war_map *map = info->war_map;
        map->timeout();
        return 0;
    }

    void war_map::team_data::initialize() {
        id = 0;
        guild = nullptr;
        member_count = 0;
        use_potion_price = 0;
        score = 0;
        chr_flag = nullptr;
        chr_flag_base = nullptr;

        pid_joiners.clear();
    }

    war_map::war_map(int32_t map_index_value, const guild_war_info &info_value, war_map_info *war_map_info_value, uint32_t guild_id1, uint32_t guild_id2) {
        map_info_ = *war_map_info_value;
        map_info_.map_index = map_index_value;

        memcpy(&war_info_, &info_value, sizeof(guild_war_info));

        team_data_[0].initialize();
        team_data_[0].id = guild_id1;
        team_data_[0].guild = get_guild_manager().touch_guild(guild_id1);

        team_data_[1].initialize();
        team_data_[1].id = guild_id2;
        team_data_[1].guild = get_guild_manager().touch_guild(guild_id2);
        observer_count_ = 0;

        auto *info = alloc_event_info<war_map_event_info>();
        info->war_map = this;

        set_begin_event(event_create(war_begin_event, info, passes_per_sec(60)));
        end_event_ = nullptr;
        timeout_event_ = nullptr;
        reset_flag_event_ = nullptr;
        timeout_ = false;
        start_time_ = get_dword_time();
        ended_ = false;

        if (type() == WAR_MAP_TYPE_FLAG) {
            add_flag_base(0);
            add_flag_base(1);
            add_flag(0);
            add_flag(1);
        }
    }

    war_map::~war_map() {
        event_cancel(&begin_event_);
        event_cancel(&end_event_);
        event_cancel(&timeout_event_);
        event_cancel(&reset_flag_event_);

        metin2::log::info("Guild war: closing war map {:d}", map_index());

        auto it = chrs_.begin();

        while (it != chrs_.end()) {
            character *ch = *(it++);

            if (ch->get_desc()) {
                metin2::log::info("Guild war: disconnecting player {} from closing war map", ch->name());
                get_desc_manager().destroy_desc(ch->get_desc());
            }
        }

        chrs_.clear();
    }

    namespace {
        void replace_event(event_ptr &slot, event_ptr ev) {
            if (slot != nullptr) {
                event_cancel(&slot);
            }
            if (ev != nullptr) {
                slot = ev;
            }
        }
    }

    void war_map::set_begin_event(event_ptr ev) {
        replace_event(begin_event_, ev);
    }

    void war_map::set_end_event(event_ptr ev) {
        replace_event(end_event_, ev);
    }

    void war_map::set_timeout_event(event_ptr ev) {
        replace_event(timeout_event_, ev);
    }

    void war_map::set_reset_flag_event(event_ptr ev) {
        replace_event(reset_flag_event_, ev);
    }

    auto war_map::team_index(uint32_t guild_id, uint8_t &idx) -> bool {
        if (team_data_[0].id == guild_id) {
            idx = 0;
            return true;
        } else if (team_data_[1].id == guild_id) {
            idx = 1;
            return true;
        }

        return false;
    }

    auto war_map::get_guild(uint8_t idx) -> guild * {
        return team_data_[idx].guild;
    }

    auto war_map::map_index() -> int32_t {
        return map_info_.map_index;
    }

    auto war_map::type() -> uint8_t {
        return map_info_.type;
    }

    auto war_map::guild_opponent(character *ch) -> uint32_t {
        if (ch->get_guild()) {
            uint32_t gid = ch->get_guild()->id();
            uint8_t idx;

            if (team_index(gid, idx))
                return team_data_[!idx].id;
        }
        return 0;
    }

    auto war_map::winner_guild() -> uint32_t {
        uint32_t win_gid = 0;

        if (team_data_[1].score > team_data_[0].score) {
            win_gid = team_data_[1].id;
        } else if (team_data_[0].score > team_data_[1].score) {
            win_gid = team_data_[0].id;
        }

        return (win_gid);
    }

    void war_map::use_potion(character *ch, item *item) {
        if (end_event_)
            return;

        if (ch->is_observer_mode())
            return;

        if (!ch->get_guild())
            return;

        if (!item->proto())
            return;

        int price = item->proto()->gold;

        uint32_t gid = ch->get_guild()->id();

        if (gid == team_data_[0].id)
            team_data_[0].use_potion_price += price;
        else if (gid == team_data_[1].id)
            team_data_[1].use_potion_price += price;
    }

    auto war_map::team_data::accumulated_joiner_count() -> int {
        return pid_joiners.size();
    }

    auto war_map::team_data::cur_jointer_count() -> int {
        return member_count;
    }

    void war_map::team_data::append_member(character *ch) {
        pid_joiners.insert(ch->player_id());
        ++member_count;
    }

    void war_map::team_data::remove_member(character *ch) {
        // The joiner set is cumulative; never remove from it.
        --member_count;
    }

    void war_map::update_user_count() {
        const std::string observer_text = std::format("ObserverCount {}", observer_count_);
        const std::string user_count_text = std::format("WarUC {} {} {} {} {}", team_data_[0].id, team_data_[0].accumulated_joiner_count(), team_data_[1].id, team_data_[1].accumulated_joiner_count(), observer_count_);

        for (character *ch : chrs_) {
            ch->chat_packet(CHAT_TYPE_COMMAND, observer_text.c_str());
            ch->chat_packet(CHAT_TYPE_COMMAND, user_count_text.c_str());
        }
    }

    void war_map::inc_member(character *ch) {
        if (!ch->is_pc())
            return;

        uint32_t gid = 0;

        if (ch->get_guild())
            gid = ch->get_guild()->id();

        bool is_war_member = ch->quest_flag("war.is_war_member") > 0 ? true : false;

        if (is_war_member && gid != team_data_[0].id && gid != team_data_[1].id) {
            ch->set_quest_flag("war.is_war_member", 0);
            is_war_member = false;
        }

        if (is_war_member) {
            if (gid == team_data_[0].id) {
                team_data_[0].append_member(ch);

            } else if (gid == team_data_[1].id) {
                team_data_[1].append_member(ch);
            }

            event_cancel(&timeout_event_);

            metin2::log::debug("Guild war: member joined, guild {:d} ({:d} present, {:d} total) vs guild {:d} ({:d} present, {:d} total)",
                team_data_[0].id, team_data_[0].cur_jointer_count(), team_data_[0].accumulated_joiner_count(),
                team_data_[1].id, team_data_[1].cur_jointer_count(), team_data_[1].accumulated_joiner_count());
        } else {
            ++observer_count_;
            metin2::log::debug("Guild war: observer joined, {:d} observers", observer_count_);
            ch->set_observer_mode(true);
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("관전 모드로 길드전에 참가하셨습니다."));
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("자신을 선택하시면 밖으로 나갈 수 있는 <관람 종료> 버튼이 나옵니다."));
        }

        update_user_count();

        chrs_.insert(ch);

        desc *d = ch->get_desc();

        send_war_packet(d);
        send_score_packet(0, d);
        send_score_packet(1, d);
    }

    void war_map::dec_member(character *ch) {
        if (!ch->is_pc())
            return;

        uint32_t gid = 0;

        if (ch->get_guild())
            gid = ch->get_guild()->id();

        if (!ch->is_observer_mode()) {
            if (gid == team_data_[0].id)
                team_data_[0].remove_member(ch);
            else if (gid == team_data_[1].id)
                team_data_[1].remove_member(ch);

            if (map_info_.type == WAR_MAP_TYPE_FLAG) {
                affect *aff = ch->find_affect(AFFECT_WAR_FLAG);

                if (aff) {
                    uint8_t idx;

                    if (team_index(aff->apply_value, idx))
                        add_flag(idx, ch->x(), ch->y());

                    ch->remove_affect(AFFECT_WAR_FLAG);
                }
            }

            metin2::log::debug("Guild war: member left, guild {:d} ({:d} present, {:d} total) vs guild {:d} ({:d} present, {:d} total)",
                team_data_[0].id, team_data_[0].cur_jointer_count(), team_data_[0].accumulated_joiner_count(),
                team_data_[1].id, team_data_[1].cur_jointer_count(), team_data_[1].accumulated_joiner_count());

            check_war_end();
            ch->set_quest_flag("war.is_war_member", 0);
        } else {
            --observer_count_;

            metin2::log::debug("Guild war: observer left, {:d} observers", observer_count_);
            ch->set_observer_mode(false);
        }

        update_user_count();

        chrs_.erase(ch);
    }

    void war_map::exit_all() {
        for (character *ch : chrs_) {
            if (ch->is_pc()) {
                ch->exit_to_saved_location();
            }
        }
    }

    void war_map::check_war_end() {
        if (ended_)
            return;

        if (team_data_[0].member_count == 0 || team_data_[1].member_count == 0) {
            if (timeout_)
                return;

            if (timeout_event_)
                return;

            notice(lc_text("길드전에 참가한 상대방 길드원이 아무도 없습니다."));
            notice(lc_text("1분 이내에 아무도 접속하지 않으면 길드전이 자동 종료됩니다."));

            metin2::log::info("Guild war: guild {:d} vs {:d} has an empty side, ending in one minute unless someone joins", team_data_[0].id, team_data_[1].id);

            auto *info = alloc_event_info<war_map_event_info>();
            info->war_map = this;

            set_timeout_event(event_create(war_timeout_event, info, passes_per_sec(60)));
        } else
            check_score();
    }

    auto war_map::reward_gold(uint8_t winner_idx) -> int {
        int reward_gold = war_info_.war_price;
        reward_gold += (team_data_[winner_idx].use_potion_price * war_info_.winner_potion_reward_pct_to_winner) / 100;
        reward_gold += (team_data_[winner_idx ? 0 : 1].use_potion_price * war_info_.loser_potion_reward_pct_to_winner) / 100;
        return reward_gold;
    }

    void war_map::draw() {
        get_guild_manager().request_war_over(team_data_[0].id, team_data_[1].id, 0, 0);
    }

    void war_map::timeout() {
        set_timeout_event(nullptr);

        if (timeout_)
            return;

        if (ended_)
            return;

        uint32_t winner_value = 0;
        uint32_t loser_value = 0;
        int reward_gold_value = 0;

        if (get_dword_time() - start_time_ < 60000 * 5) {
            notice(lc_text("길드전이 일찍 종료되어 무승부로 판정 되었습니다. (5분이 지나지 않음)"));
            winner_value = 0;
            loser_value = 0;
        } else {
            int winner_idx = -1;

            if (team_data_[0].member_count == 0)
                winner_idx = 1;
            else if (team_data_[1].member_count == 0)
                winner_idx = 0;

            if (winner_idx == -1) {
                winner_value = winner_guild();

                if (winner_value == team_data_[0].id) {
                    reward_gold_value = reward_gold(0);
                    loser_value = team_data_[1].id;
                } else if (winner_value == team_data_[1].id) {
                    reward_gold_value = reward_gold(1);
                    loser_value = team_data_[0].id;
                }

                metin2::log::info("Guild war: timed out with both sides present, guild {:d} ({:d} members) vs guild {:d} ({:d} members), winner by score {:d}",
                    team_data_[0].id, team_data_[0].member_count,
                    team_data_[1].id, team_data_[1].member_count,
                    winner_value);
            } else {
                winner_value = team_data_[winner_idx].id;
                loser_value = team_data_[winner_idx == 0 ? 1 : 0].id;

                reward_gold_value = reward_gold(winner_idx);
            }
        }

        metin2::log::info("Guild war: guild {:d} vs {:d} timed out, winner {:d}, loser {:d}, reward {:d} gold, map {:d}",
            team_data_[0].id, team_data_[1].id, winner_value, loser_value, reward_gold_value, map_info_.map_index);

        if (winner_value)
            get_guild_manager().request_war_over(winner_value, loser_value, winner_value, reward_gold_value);
        else
            get_guild_manager().request_war_over(team_data_[0].id, team_data_[1].id, winner_value, reward_gold_value);

        timeout_ = true;
    }

    void war_map::notice(const char *psz) {
        for (character *ch : chrs_)
            ch->chat_packet(CHAT_TYPE_NOTICE, "%s", psz);
    }

    void war_map::packet(const void *p, int size) {
        for (character *ch : chrs_)
            ch->get_desc()->packet(p, size);
    }

    void war_map::send_war_packet(desc *d) {
        packet_gc_guild pack;
        packet_gc_guild_war pack2;

        pack.header = HEADER_GC_GUILD;
        pack.subheader = GUILD_SUBHEADER_GC_WAR;
        pack.size = sizeof(pack) + sizeof(pack2);

        pack2.guild_self = team_data_[0].id;
        pack2.guild_opp = team_data_[1].id;
        pack2.type = get_guild_manager().touch_guild(team_data_[0].id)->get_guild_war_type(team_data_[1].id);
        pack2.war_state = get_guild_manager().touch_guild(team_data_[0].id)->get_guild_war_state(team_data_[1].id);

        d->buffered_packet(&pack, sizeof(pack));
        d->packet(&pack2, sizeof(pack2));
    }

    void war_map::send_score_packet(uint8_t idx, desc *d) {
        packet_gc_guild p;

        p.header = HEADER_GC_GUILD;
        p.subheader = GUILD_SUBHEADER_GC_WAR_SCORE;
        p.size = sizeof(p) + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(int32_t);

        temp_buffer buf;
        buf.write(&p, sizeof(p));
        buf.write(&team_data_[idx].id, sizeof(uint32_t));
        buf.write(&team_data_[idx ? 0 : 1].id, sizeof(uint32_t));
        buf.write(&team_data_[idx].score, sizeof(int32_t));

        if (d)
            d->packet(buf.read_peek(), buf.size());
        else
            packet(buf.read_peek(), buf.size());
    }

    void war_map::update_score(uint32_t g1, int score1, uint32_t g2, int score2) {
        uint8_t idx;

        if (team_index(g1, idx)) {
            if (team_data_[idx].score != score1) {
                team_data_[idx].score = score1;
                send_score_packet(idx);
            }
        }

        if (team_index(g2, idx)) {
            if (team_data_[idx].score != score2) {
                team_data_[idx].score = score2;
                send_score_packet(idx);
            }
        }

        check_score();
    }

    auto war_map::check_score() -> bool {
        if (ended_)
            return true;

        if (get_dword_time() - start_time_ < 30000)
            return false;

        if (team_data_[0].score == team_data_[1].score)
            return false;

        int end_score = war_info_.end_score;

        if (metin2::config::test_server()) end_score /= 10;

        uint32_t winner_value;
        uint32_t loser;

        if (team_data_[0].score >= end_score) {
            winner_value = team_data_[0].id;
            loser = team_data_[1].id;
        } else if (team_data_[1].score >= end_score) {
            winner_value = team_data_[1].id;
            loser = team_data_[0].id;
        } else
            return false;

        int reward_gold_value = 0;

        if (winner_value == team_data_[0].id)
            reward_gold_value = reward_gold(0);
        else if (winner_value == team_data_[1].id)
            reward_gold_value = reward_gold(1);

        metin2::log::info("Guild war: end score {:d} reached, guild {:d} score {:d} vs guild {:d} score {:d}, winner {:d}, reward {:d} gold",
            end_score,
            team_data_[0].id,
            team_data_[0].score,
            team_data_[1].id,
            team_data_[1].score,
            winner_value,
            reward_gold_value);

        get_guild_manager().request_war_over(winner_value, loser, winner_value, reward_gold_value);
        return true;
    }

    auto war_map::set_ended() -> bool {
        metin2::log::info("Guild war: war on map {:d} ended", map_info_.map_index);

        if (end_event_)
            return false;

        if (team_data_[0].chr_flag) {
            get_character_manager().destroy_character(team_data_[0].chr_flag);
            team_data_[0].chr_flag = nullptr;
        }

        if (team_data_[0].chr_flag_base) {
            get_character_manager().destroy_character(team_data_[0].chr_flag_base);
            team_data_[0].chr_flag_base = nullptr;
        }

        if (team_data_[1].chr_flag) {
            get_character_manager().destroy_character(team_data_[1].chr_flag);
            team_data_[1].chr_flag = nullptr;
        }

        if (team_data_[1].chr_flag_base) {
            get_character_manager().destroy_character(team_data_[1].chr_flag_base);
            team_data_[1].chr_flag_base = nullptr;
        }

        event_cancel(&reset_flag_event_);
        ended_ = true;

        auto *info = alloc_event_info<war_map_event_info>();

        info->war_map = this;
        info->step = 0;
        set_end_event(event_create(war_end_event, info, passes_per_sec(10)));
        return true;
    }

    void war_map::on_kill(character *killer, character *ch) {
        if (ended_)
            return;

        uint32_t killer_guild = 0;
        uint32_t dead_guild = 0;

        if (killer->get_guild())
            killer_guild = killer->get_guild()->id();

        if (ch->get_guild())
            dead_guild = ch->get_guild()->id();

        uint8_t idx;

        metin2::log::debug("Guild war: kill, killer guild {:d}, dead guild {:d}", killer_guild, dead_guild);

        if (!team_index(killer_guild, idx))
            return;

        if (!team_index(dead_guild, idx))
            return;

        switch (map_info_.type) {
        case WAR_MAP_TYPE_NORMAL:
            send_guild_war_score(killer_guild, dead_guild, 1, ch->level());
            break;

        case WAR_MAP_TYPE_FLAG:
            {
                affect *aff = ch->find_affect(AFFECT_WAR_FLAG);

                if (aff) {
                    if (team_index(aff->apply_value, idx))
                        add_flag(idx, ch->x(), ch->y());

                    ch->remove_affect(AFFECT_WAR_FLAG);
                }
            }
            break;

        default:
            metin2::log::error("Unknown war map type {:d} on map {:d}", map_info_.type, map_info_.map_index);
            break;
        }
    }

    void war_map::add_flag_base(uint8_t idx, uint32_t x, uint32_t y) {
        if (ended_)
            return;

        assert(idx < 2);

        team_data &r = team_data_[idx];

        if (r.chr_flag_base)
            return;

        if (x == 0) {
            x = map_info_.pos_start[idx].x;
            y = map_info_.pos_start[idx].y;
        }

        r.chr_flag_base = get_character_manager().spawn_mob(warmap::WAR_FLAG_BASE_VNUM, map_info_.map_index, x, y, 0);
        metin2::log::debug("Guild war: spawned flag base for team {:d} ({}), guild {:d}", idx, static_cast<const void *>(r.chr_flag_base), r.id);

        r.chr_flag_base->set_point(POINT_STAT, r.id);
        r.chr_flag_base->set_war_map(this);
    }

    void war_map::add_flag(uint8_t idx, uint32_t x, uint32_t y) {
        if (ended_)
            return;

        assert(idx < 2);

        team_data &r = team_data_[idx];

        if (r.chr_flag)
            return;

        if (x == 0) {
            x = map_info_.pos_start[idx].x;
            y = map_info_.pos_start[idx].y;
        }

        r.chr_flag = get_character_manager().spawn_mob(idx == 0 ? warmap::WAR_FLAG_VNUM0 : warmap::WAR_FLAG_VNUM1, map_info_.map_index, x, y, 0);
        metin2::log::debug("Guild war: spawned flag for team {:d} ({}), guild {:d}", idx, static_cast<const void *>(r.chr_flag), r.id);

        r.chr_flag->set_point(POINT_STAT, r.id);
        r.chr_flag->set_war_map(this);
    }

    void war_map::remove_flag(uint8_t idx) {
        assert(idx < 2);

        team_data &r = team_data_[idx];

        if (!r.chr_flag)
            return;

        metin2::log::debug("Guild war: removing flag of team {:d} ({})", idx, static_cast<const void *>(r.chr_flag));

        r.chr_flag->dead(nullptr, true);
        r.chr_flag = nullptr;
    }

    auto war_map::is_flag_on_base(uint8_t idx) -> bool {
        assert(idx < 2);

        team_data &r = team_data_[idx];

        if (!r.chr_flag)
            return false;

        const pixel_position &pos = r.chr_flag->xyz();

        if (pos.x == map_info_.pos_start[idx].x && pos.y == map_info_.pos_start[idx].y)
            return true;

        return false;
    }

    EVENTFUNC(war_reset_flag_event) {
        auto *info = dynamic_cast<war_map_event_info *>(event->info.get());

        if (info == nullptr) {
            metin2::log::error("Guild war flag reset event has no event info");
            return 0;
        }

        war_map *map = info->war_map;

        map->add_flag(0);
        map->add_flag(1);

        map->set_reset_flag_event(nullptr);
        return 0;
    }

    void war_map::reset_flag() {
        if (map_info_.type != WAR_MAP_TYPE_FLAG)
            return;

        if (reset_flag_event_)
            return;

        if (ended_)
            return;

        for (character *ch : chrs_) {
            if (ch->find_affect(AFFECT_WAR_FLAG))
                ch->remove_affect(AFFECT_WAR_FLAG);
        }

        remove_flag(0);
        remove_flag(1);

        auto *info = alloc_event_info<war_map_event_info>();

        info->war_map = this;
        info->step = 0;
        set_reset_flag_event(event_create(war_reset_flag_event, info, passes_per_sec(10)));
    }

    war_map_manager::war_map_manager() = default;

    war_map_manager::~war_map_manager() = default;

    auto war_map_manager::load_war_map_info(const char *file_name) -> bool {
        auto k = std::make_unique<war_map_info>();
        k->type = WAR_MAP_TYPE_NORMAL;

        k->map_index = 110;
        k->pos_start[0].x = 48 * 100 + 32000;
        k->pos_start[0].y = 52 * 100 + 0;
        k->pos_start[1].x = 183 * 100 + 32000;
        k->pos_start[1].y = 206 * 100 + 0;
        k->pos_start[2].x = 141 * 100 + 32000;
        k->pos_start[2].y = 117 * 100 + 0;

        war_map_info_.emplace(k->map_index, std::move(k));

        k = std::make_unique<war_map_info>();
        k->type = WAR_MAP_TYPE_FLAG;

        k->map_index = 111;
        k->pos_start[0].x = 68 * 100 + 57600;
        k->pos_start[0].y = 69 * 100 + 0;
        k->pos_start[1].x = 171 * 100 + 57600;
        k->pos_start[1].y = 182 * 100 + 0;
        k->pos_start[2].x = 122 * 100 + 57600;
        k->pos_start[2].y = 131 * 100 + 0;

        war_map_info_.emplace(k->map_index, std::move(k));
        return true;
    }

    auto war_map_manager::is_war_map(int32_t map_index) -> bool {
        return get_war_map_info(map_index) ? true : false;
    }

    auto war_map_manager::start_position(int32_t map_index, uint8_t idx, pixel_position &pos) -> bool {
        assert(idx < 3);

        war_map_info *pi = get_war_map_info(map_index);

        if (!pi) {
            metin2::log::warn("No war map info for map {:d}, cannot get start position ({:d} war maps registered)", map_index, war_map_info_.size());

            decltype(war_map_info_.begin()) it;
            for (it = war_map_info_.begin(); it != war_map_info_.end(); ++it) {
                pixel_position &cur = it->second->pos_start[idx];
                metin2::log::warn("War map {:d} start position ({:d}, {:d})", it->first, cur.x, cur.y);
            }
            return false;
        }

        pos = pi->pos_start[idx];
        return true;
    }

    auto war_map_manager::create_war_map(const guild_war_info &guild_war_info_value, uint32_t guild_id1, uint32_t guild_id2) -> int32_t {
        war_map_info *info_value = get_war_map_info(guild_war_info_value.map_index);
        if (!info_value) {
            metin2::log::error("No war map info for map {:d}, cannot create guild war map", guild_war_info_value.map_index);
            return 0;
        }

        uint32_t map_index = get_sectree_manager().create_private_map(guild_war_info_value.map_index);

        if (map_index) {
            war_map_.insert(std::make_pair(map_index, new war_map(map_index, guild_war_info_value, info_value, guild_id1, guild_id2)));
        }

        return map_index;
    }

    auto war_map_manager::get_war_map_info(int32_t map_index) -> war_map_info * {
        if (map_index >= 10000)
            map_index /= 10000;

        auto it = war_map_info_.find(map_index);

        if (war_map_info_.end() == it)
            return nullptr;

        return it->second.get();
    }

    void war_map_manager::destroy_war_map(war_map *map) {
        int32_t map_idx = map->map_index();

        metin2::log::info("Guild war: destroying war map {:d}", map_idx);

        war_map_.erase(map->map_index());
        delete map;

        get_sectree_manager().destroy_private_map(map_idx);
    }

    auto war_map_manager::find(int32_t map_index) -> war_map * {
        auto it = war_map_.find(map_index);

        if (it == war_map_.end())
            return nullptr;

        return it->second;
    }

    void war_map_manager::on_shutdown() {
        auto it = war_map_.begin();

        while (it != war_map_.end())
            (it++)->second->draw();
    }
}
