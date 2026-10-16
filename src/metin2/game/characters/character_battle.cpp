#include <metin2/config/config.h>
#include <metin2/game/app/config.h>
#include <metin2/game/app/constants.h>
#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/characters/affect.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/characters/mob_manager.h>
#include <metin2/game/combat/battle.h>
#include <metin2/game/combat/pvp.h>
#include <metin2/game/combat/skill.h>
#include <metin2/game/empire/castle.h>
#include <metin2/game/empire/monarch.h>
#include <metin2/game/empire/priv_manager.h>
#include <metin2/game/empire/start_position.h>
#include <metin2/game/empire/threeway_war.h>
#include <metin2/game/events/arena.h>
#include <metin2/game/events/battle_arena.h>
#include <metin2/game/events/blue_dragon.h>
#include <metin2/game/events/dragon_lair.h>
#include <metin2/game/guilds/guild.h>
#include <metin2/game/guilds/guild_manager.h>
#include <metin2/game/items/cube.h>
#include <metin2/game/items/item.h>
#include <metin2/game/items/item_manager.h>
#include <metin2/game/items/unique_item.h>
#include <metin2/game/parties/party.h>
#include <metin2/game/persistence/db.h>
#include <metin2/game/persistence/log.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/quest/quest.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/social/marriage.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/trade/exchange.h>
#include <metin2/game/trade/shop_manager.h>
#include <metin2/game/world/dungeon.h>
#include <metin2/game/world/regen.h>
#include <metin2/game/world/sectree.h>
#include <metin2/game/world/sectree_manager.h>
#include <metin2/game/world/vector.h>
#include <metin2/game/world/vid.h>

#include <metin2/core/main.h>
#include <metin2/core/utils.h>
#include <metin2/log/log.h>
#include <metin2/proto/character_types.h>
#include <metin2/proto/chat_types.h>
#include <metin2/proto/guild_types.h>
#include <metin2/proto/inventory.h>
#include <metin2/proto/item_types.h>
#include <metin2/proto/log_types.h>
#include <metin2/proto/mob_types.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <format>
#include <map>
#include <queue>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace metin2::game::inline characters {

    auto adjust_exp_by_level(character *const ch, const uint32_t exp) -> uint32_t {
        if (PLAYER_EXP_TABLE_MAX < ch->level()) {
            double ret = 0.95;
            double factor = 0.1;

            for (ssize_t i = 0; i < ch->level() - 100; ++i) {
                if ((i % 10) == 0)
                    factor /= 2.0;

                ret *= 1.0 - factor;
            }

            ret = ret * static_cast<double>(exp);

            if (ret < 1.0)
                return 1;

            return static_cast<uint32_t>(ret);
        }

        return exp;
    }

    auto character::can_begin_fight() const -> bool {
        if (!can_move())
            return false;

        return points_instant_.position == POS_STANDING && !is_dead() && !is_stun();
    }

    void character::begin_fight(character *victim) {
        set_victim(victim);
        set_position(POS_FIGHTING);
        set_next_state_pulse(1);
    }

    void character::create_fly(uint8_t type_value, character *victim) {
        packet_gc_create_fly pack_fly;

        pack_fly.header = HEADER_GC_CREATE_FLY;
        pack_fly.type = type_value;
        pack_fly.start_vid = get_vid();
        pack_fly.end_vid = victim->get_vid();

        packet_around(&pack_fly, sizeof(packet_gc_create_fly));
    }

    void character::distribute_sp(character *killer, int method) {
        if (killer->sp() >= killer->max_sp())
            return;

        bool attacking = (get_dword_time() - last_attack_time()) < 3000;
        bool moving = (get_dword_time() - last_move_time()) < 3000;

        if (method == 1) {
            int num = number(0, 3);

            if (!num) {
                int lv_delta = level() - killer->level();
                int amount = 0;

                if (lv_delta >= 5)
                    amount = 10;
                else if (lv_delta >= 0)
                    amount = 6;
                else if (lv_delta >= -3)
                    amount = 2;

                if (amount != 0) {
                    amount += (amount * killer->point(POINT_SP_REGEN)) / 100;

                    if (amount >= 11)
                        create_fly(FLY_SP_BIG, killer);
                    else if (amount >= 7)
                        create_fly(FLY_SP_MEDIUM, killer);
                    else
                        create_fly(FLY_SP_SMALL, killer);

                    killer->point_change(POINT_SP, amount);
                }
            }
        } else {
            if (killer->job() == JOB_SHAMAN || (killer->job() == JOB_SURA && killer->skill_group() == 2)) {
                int amount;

                if (attacking)
                    amount = 2 + max_sp() / 100;
                else if (moving)
                    amount = 3 + max_sp() * 2 / 100;
                else
                    amount = 10 + max_sp() * 3 / 100;

                amount += (amount * killer->point(POINT_SP_REGEN)) / 100;
                killer->point_change(POINT_SP, amount);
            } else {
                int amount;

                if (attacking)
                    amount = 2 + killer->max_sp() / 200;
                else if (moving)
                    amount = 2 + killer->max_sp() / 100;
                else {
                    if (killer->hp() < killer->max_hp())
                        amount = 2 + (killer->max_sp() / 100);
                    else
                        amount = 9 + (killer->max_sp() / 100);
                }

                amount += (amount * killer->point(POINT_SP_REGEN)) / 100;
                killer->point_change(POINT_SP, amount);
            }
        }
    }

    auto character::attack(character *victim, uint8_t type) -> bool {
        if (metin2::config::test_server())
            metin2::log::debug("{}: attack type {:d}, mob attack range {:d}", name(), type, !mob_battle_type() ? 0 : mob_attack_range());
        if (!can_move())
            return false;

        if (is_castle_map(map_index()) && false == castle_can_attack(this, victim))
            return false;

        uint32_t current_time = get_dword_time();

        if (is_pc()) {
            if (is_speed_hack(this, victim, current_time))
                return false;

            if (type == 0 && current_time < get_skip_combo_attack_by_time())
                return false;
        } else {
            monster_chat(MONSTER_CHAT_ATTACK);
        }

        victim->set_sync_owner(this);

        if (victim->can_begin_fight())
            victim->begin_fight(this);

        int ret;

        if (type == 0) {
            switch (mob_battle_type()) {
            case BATTLE_TYPE_MELEE:
            case BATTLE_TYPE_POWER:
            case BATTLE_TYPE_TANKER:
            case BATTLE_TYPE_SUPER_POWER:
            case BATTLE_TYPE_SUPER_TANKER:
                ret = battle_melee_attack(this, victim);
                break;

            case BATTLE_TYPE_RANGE:
                fly_target(victim->get_vid(), victim->x(), victim->y(), HEADER_CG_FLY_TARGETING);
                ret = shoot(0) ? BATTLE_DAMAGE : BATTLE_NONE;
                break;

            case BATTLE_TYPE_MAGIC:
                fly_target(victim->get_vid(), victim->x(), victim->y(), HEADER_CG_FLY_TARGETING);
                ret = shoot(1) ? BATTLE_DAMAGE : BATTLE_NONE;
                break;

            default:
                metin2::log::error("Unhandled mob battle type {:d}", mob_battle_type());
                ret = BATTLE_NONE;
                break;
            }
        } else {
            if (is_pc() == true) {
                if (current_time - last_skill_time_ > 1500) {
                    metin2::log::warn("Possible skill hack by {} (pid {:d}): skill attack {:d} ms after skill use",
                        name(), player_id(), (current_time - last_skill_time_));
                    return false;
                }
            }

            metin2::log::debug("Attack computes skill {:d} on {}", type, victim ? victim->name() : "");
            ret = compute_skill(type, victim);
        }

        if (ret == BATTLE_DAMAGE || ret == BATTLE_DEAD) {
            on_move(true);
            victim->on_move();

            if (BATTLE_DEAD == ret && is_pc())
                set_victim(nullptr);

            return true;
        }

        return false;
    }

    void character::death_penalty(uint8_t town) {
        metin2::log::debug("{}: death penalty check (town {:d})", name(), town);

        cube_close(this);

        if (get_battle_arena().is_battle_arena_map(map_index()) == true) {
            return;
        }

        if (level() < 10) {
            metin2::log::info("{}: no death penalty (below level 10)", name());
            chat_packet(CHAT_TYPE_INFO, lc_text("용신의 가호로 경험치가 떨어지지 않았습니다."));
            return;
        }

        if (number(0, 2)) {
            metin2::log::info("{}: no death penalty (luck)", name());
            chat_packet(CHAT_TYPE_INFO, lc_text("용신의 가호로 경험치가 떨어지지 않았습니다."));
            return;
        }

        if (is_set(points_instant_.instant_flag, INSTANT_FLAG_DEATH_PENALTY)) {
            remove_bit(points_instant_.instant_flag, INSTANT_FLAG_DEATH_PENALTY);

            if (!town) {
                if (find_affect(AFFECT_NO_DEATH_PENALTY)) {
                    metin2::log::info("{}: no death penalty (protection affect)", name());
                    chat_packet(CHAT_TYPE_INFO, lc_text("용신의 가호로 경험치가 떨어지지 않았습니다."));
                    remove_affect(AFFECT_NO_DEATH_PENALTY);
                    return;
                }
            }

            int loss = ((next_exp() * exp_loss_percents[std::clamp(level(), 1, PLAYER_EXP_TABLE_MAX)]) / 100);

            loss = std::min(800000, loss);

            if (town) {
                loss = 0;
            }

            if (is_equip_unique_item(UNIQUE_ITEM_TEARDROP_OF_GODNESS))
                loss /= 2;

            metin2::log::info("{}: death penalty, lost {:d} exp ({:d}%)", name(), loss, exp_loss_percents[std::min(metin2::config::game().max_level, level())]);

            point_change(POINT_EXP, -loss, true);
        }
    }

    auto character::is_stun() const -> bool {
        if (is_set(points_instant_.instant_flag, INSTANT_FLAG_STUN))
            return true;

        return false;
    }

    EVENTFUNC(stun_event) {
        auto *info = dynamic_cast<char_event_info *>(event->info.get());

        if (info == nullptr) {
            metin2::log::error("Stun event fired without event info");
            return 0;
        }

        character *ch = info->ch;

        if (ch == nullptr) {
            return 0;
        }
        ch->stun_event = nullptr;
        ch->dead();
        return 0;
    }

    void character::stun() {
        if (is_stun())
            return;

        if (is_dead())
            return;

        if (!is_pc() && party_) {
            party_->send_message(this, PM_ATTACKED_BY, 0, 0);
        }

        metin2::log::debug("{} ({}) stunned", name(), static_cast<const void *>(this));

        point_change(POINT_HP_RECOVERY, -point(POINT_HP_RECOVERY));
        point_change(POINT_SP_RECOVERY, -point(POINT_SP_RECOVERY));

        close_my_shop();

        event_cancel(&recovery_event);

        packet_gc_stun pack;
        pack.header = HEADER_GC_STUN;
        pack.vid = vid_;
        packet_around(&pack, sizeof(pack));

        set_bit(points_instant_.instant_flag, INSTANT_FLAG_STUN);

        if (stun_event)
            return;

        auto *info = alloc_event_info<char_event_info>();

        info->ch = this;

        stun_event = event_create(game::characters::stun_event, info, passes_per_sec(3));
    }

    EVENTINFO(char_dead_event_info) {
        bool is_pc;
        uint32_t id;

        char_dead_event_info()
            : is_pc(0), id(0) {
        }
    };

    EVENTFUNC(dead_event) {
        const char_dead_event_info *info = dynamic_cast<char_dead_event_info *>(event->info.get());

        if (info == nullptr) {
            metin2::log::error("Dead event fired without event info");
            return 0;
        }

        character *ch = nullptr;

        if (true == info->is_pc) {
            ch = get_character_manager().find_by_pid(info->id);
        } else {
            ch = get_character_manager().find(info->id);
        }

        if (nullptr == ch) {
            metin2::log::error("Dead event: cannot find {} with id {:d}", info->is_pc ? "player" : "mob", info->id);
            return 0;
        }

        ch->dead_event = nullptr;

        if (ch->get_desc()) {
            ch->get_desc()->set_phase(PHASE_GAME);

            ch->set_position(POS_STANDING);

            pixel_position pos;

            if (get_sectree_manager().recall_position_by_empire(ch->map_index(), ch->empire(), pos))
                ch->warp_set(pos.x, pos.y);
            else {
                metin2::log::error("Cannot find spawn position for {}", ch->name());
                ch->warp_set(empire_start_x(ch->empire()), empire_start_y(ch->empire()));
            }

            ch->point_change(POINT_HP, (ch->max_hp() / 2) - ch->hp(), true);

            ch->death_penalty(0);

            ch->start_recovery_event();

            ch->chat_packet(CHAT_TYPE_COMMAND, "CloseRestartWindow");
        } else {
            if (ch->is_monster() == true) {
                if (ch->is_revive() == false && ch->has_reviver_in_party() == true) {
                    ch->set_position(POS_STANDING);
                    ch->set_hp(ch->max_hp());

                    ch->view_reencode();

                    ch->set_aggressive();
                    ch->set_revive(true);

                    return 0;
                }
            }

            get_character_manager().destroy_character(ch);
        }

        return 0;
    }

    auto character::is_dead() const -> bool {
        if (points_instant_.position == POS_DEAD)
            return true;

        return false;
    }

    static auto get_gold_multipler() -> int { return metin2::config::game().distribution_test_server ? 3 : 1; }

    void character::reward_gold(character *attacker) {
        bool is_auto_loot =
            (attacker->premium_remain_seconds(PREMIUM_AUTOLOOT) > 0 ||
                attacker->is_equip_unique_group(UNIQUE_GROUP_AUTOLOOT))
                ? true
                : false;

        pixel_position pos;

        if (!is_auto_loot)
            if (!get_sectree_manager().movable_position(map_index(), x(), y(), pos))
                return;

        int total_gold = 0;
        int gold_percent = mob_rank_stats[get_mob_rank()].gold_percent;

        if (attacker->is_pc())
            gold_percent = gold_percent * (100 + get_priv_manager().priv(attacker, PRIV_GOLD_DROP)) / 100;

        if (attacker->point(POINT_MALL_GOLDBONUS))
            gold_percent += (gold_percent * attacker->point(POINT_MALL_GOLDBONUS) / 100);

        gold_percent = gold_percent * get_character_manager().mob_gold_drop_rate(attacker) / 100;

        if (attacker->premium_remain_seconds(PREMIUM_GOLD) > 0 ||
            attacker->is_equip_unique_group(UNIQUE_GROUP_LUCKY_GOLD))
            gold_percent += gold_percent;

        if (gold_percent > 100)
            gold_percent = 100;

        int percent;

        if (get_mob_rank() >= MOB_RANK_BOSS)
            percent = ((gold_percent * percent_lvdelta_boss(attacker->level(), level())) / 100);
        else
            percent = ((gold_percent * percent_lvdelta(attacker->level(), level())) / 100);

        if (number(1, 100) > percent)
            return;

        int gold_multipler = get_gold_multipler();

        if (1 == number(1, 50000))
            gold_multipler *= 10;
        else if (1 == number(1, 10000))
            gold_multipler *= 5;

        if (attacker->point(POINT_GOLD_DOUBLE_BONUS))
            if (prob(attacker->point(POINT_GOLD_DOUBLE_BONUS)))
                gold_multipler *= 2;

        if (metin2::config::test_server())
            attacker->chat_packet(CHAT_TYPE_PARTY, "gold_mul %d rate %d", gold_multipler, get_character_manager().mob_gold_amount_rate(attacker));

        item *item;

        int gold10_drop_pct = 100;
        gold10_drop_pct = (gold10_drop_pct * 100) / (100 + get_priv_manager().priv(attacker, PRIV_GOLD10_DROP));

        if (get_mob_rank() >= MOB_RANK_BOSS && !is_stone() && get_mob_table().gold_max != 0) {
            if (1 == number(1, gold10_drop_pct))
                gold_multipler *= 10;

            int split_count = number(25, 35);

            for (int i = 0; i < split_count; ++i) {
                int gold = number(get_mob_table().gold_min, get_mob_table().gold_max) / split_count;
                if (metin2::config::test_server())
                    metin2::log::debug("Gold drop: base amount {:d}", gold);
                gold = gold * get_character_manager().mob_gold_amount_rate(attacker) / 100;
                gold *= gold_multipler;

                if (gold == 0) {
                    continue;
                }

                if (metin2::config::test_server()) {
                    metin2::log::debug("Gold drop: rate {:d}%, multiplier {:d}", get_character_manager().mob_gold_amount_rate(attacker), gold_multipler);
                    metin2::log::debug("Gold drop: {:d} (min {:d}, max {:d})", gold, get_mob_table().gold_min, get_mob_table().gold_max);
                }

                if ((item = get_item_manager().create_item(1, gold))) {
                    pos.x = x() + ((number(-14, 14) + number(-14, 14)) * 23);
                    pos.y = y() + ((number(-14, 14) + number(-14, 14)) * 23);

                    item->add_to_ground(map_index(), pos);
                    item->start_destroy_event();

                    total_gold += gold;
                }
            }
        } else if (1 == number(1, gold10_drop_pct)) {
            for (int i = 0; i < 10; ++i) {
                int gold = number(get_mob_table().gold_min, get_mob_table().gold_max);
                gold = gold * get_character_manager().mob_gold_amount_rate(attacker) / 100;
                gold *= gold_multipler;

                if (gold == 0) {
                    continue;
                }

                if ((item = get_item_manager().create_item(1, gold))) {
                    pos.x = x() + (number(-7, 7) * 20);
                    pos.y = y() + (number(-7, 7) * 20);

                    item->add_to_ground(map_index(), pos);
                    item->start_destroy_event();

                    total_gold += gold;
                }
            }
        } else {
            int gold = number(get_mob_table().gold_min, get_mob_table().gold_max);
            gold = gold * get_character_manager().mob_gold_amount_rate(attacker) / 100;
            gold *= gold_multipler;

            int split_count;

            if (gold >= 3)
                split_count = number(1, 3);
            else if (get_mob_rank() >= MOB_RANK_BOSS) {
                split_count = number(3, 10);

                if ((gold / split_count) == 0)
                    split_count = 1;
            } else
                split_count = 1;

            if (gold != 0) {
                total_gold += gold;

                for (int i = 0; i < split_count; ++i) {
                    if (is_auto_loot) {
                        attacker->give_gold(gold / split_count);
                    } else if ((item = get_item_manager().create_item(1, gold / split_count))) {
                        pos.x = x() + (number(-7, 7) * 20);
                        pos.y = y() + (number(-7, 7) * 20);

                        item->add_to_ground(map_index(), pos);
                        item->start_destroy_event();
                    }
                }
            }
        }

        get_db_manager().send_money_log(MONEY_LOG_MONSTER, race_num(), total_gold);
    }

    void character::reward(bool item_drop) {
        if (race_num() == 5001) {
            pixel_position pos;

            if (!get_sectree_manager().movable_position(map_index(), x(), y(), pos))
                return;

            item *item;
            int gold_value = number(get_mob_table().gold_min, get_mob_table().gold_max);
            gold_value = gold_value * get_character_manager().mob_gold_amount_rate(nullptr) / 100;
            gold_value *= get_gold_multipler();
            int split_count = number(25, 35);

            metin2::log::debug("Waegu died: gold {:d} split into {:d}", gold_value, split_count);

            for (int i = 1; i <= split_count; ++i) {
                if ((item = get_item_manager().create_item(1, gold_value / split_count))) {
                    if (i != 0) {
                        pos.x = number(-7, 7) * 20;
                        pos.y = number(-7, 7) * 20;

                        pos.x += x();
                        pos.y += y();
                    }

                    item->add_to_ground(map_index(), pos);
                    item->start_destroy_event();
                }
            }
            return;
        }

        character *attacker = distribute_exp();

        if (!attacker)
            return;

        if (attacker->is_pc()) {
            if (level() - attacker->level() >= -10) {
                if (attacker->real_alignment() < 0) {
                    if (attacker->is_equip_unique_item(UNIQUE_ITEM_FASTER_ALIGNMENT_UP_BY_KILL))
                        attacker->update_alignment(14);
                    else
                        attacker->update_alignment(7);
                } else
                    attacker->update_alignment(2);
            }

            attacker->set_quest_npcid(get_vid());
            quest::get_quest_manager().kill(attacker->player_id(), race_num());
            get_character_manager().kill_log(race_num());

            if (!number(0, 9)) {
                if (attacker->point(POINT_KILL_HP_RECOVERY)) {
                    int hp = attacker->max_hp() * attacker->point(POINT_KILL_HP_RECOVERY) / 100;
                    attacker->point_change(POINT_HP, hp);
                    create_fly(FLY_HP_SMALL, attacker);
                }

                if (attacker->point(POINT_KILL_SP_RECOVER)) {
                    int sp = attacker->max_sp() * attacker->point(POINT_KILL_SP_RECOVER) / 100;
                    attacker->point_change(POINT_SP, sp);
                    create_fly(FLY_SP_SMALL, attacker);
                }
            }
        }

        if (!item_drop)
            return;

        pixel_position pos = xyz();

        if (!get_sectree_manager().movable_position(map_index(), pos.x, pos.y, pos))
            return;

        if (metin2::config::test_server())
            metin2::log::debug("Gold reward for {}", attacker->name());
        reward_gold(attacker);

        game::items::item *item;

        static std::vector<game::items::item *> s_vec_item;
        s_vec_item.clear();

        if (get_item_manager().create_drop_item(this, attacker, s_vec_item)) {
            if (s_vec_item.size() == 0)
                ;
            else if (s_vec_item.size() == 1) {
                item = s_vec_item[0];
                item->add_to_ground(map_index(), pos);

                if (get_battle_arena().is_battle_arena_map(attacker->map_index()) == false) {
                    item->set_ownership(attacker);
                }

                item->start_destroy_event();

                pos.x = number(-7, 7) * 20;
                pos.y = number(-7, 7) * 20;
                pos.x += x();
                pos.y += y();

                metin2::log::debug("Item {} dropped at {:d} {:d} from {}", item->name(), pos.x, pos.y, name());
            } else {
                int item_idx = s_vec_item.size() - 1;

                std::priority_queue<std::pair<int, character *>> pq;

                int total_dam = 0;

                for (auto it = map_damage_.begin(); it != map_damage_.end(); ++it) {
                    int damage = it->second.total_damage;
                    if (damage > 0) {
                        character *ch = get_character_manager().find(it->first);

                        if (ch) {
                            pq.push(std::make_pair(damage, ch));
                            total_dam += damage;
                        }
                    }
                }

                std::vector<character *> v;

                while (!pq.empty() && pq.top().first * 10 >= total_dam) {
                    v.push_back(pq.top().second);
                    pq.pop();
                }

                if (v.empty()) {
                    while (item_idx >= 0) {
                        item = s_vec_item[item_idx--];

                        if (!item) {
                            metin2::log::error("Null item at drop list index {:d}", item_idx + 1);
                            continue;
                        }

                        item->add_to_ground(map_index(), pos);
                        item->start_destroy_event();

                        pos.x = number(-7, 7) * 20;
                        pos.y = number(-7, 7) * 20;
                        pos.x += x();
                        pos.y += y();

                        metin2::log::debug("Item {} dropped at {:d} {:d} by {}", item->name(), pos.x, pos.y, name());
                    }
                } else {
                    auto it = v.begin();

                    while (item_idx >= 0) {
                        item = s_vec_item[item_idx--];

                        if (!item) {
                            metin2::log::error("Null item at drop list index {:d}", item_idx + 1);
                            continue;
                        }

                        item->add_to_ground(map_index(), pos);

                        character *ch = *it;

                        if (ch->get_party())
                            ch = ch->get_party()->next_ownership(ch, x(), y());

                        ++it;

                        if (it == v.end())
                            it = v.begin();

                        if (get_battle_arena().is_battle_arena_map(ch->map_index()) == false) {
                            item->set_ownership(ch);
                        }

                        item->start_destroy_event();

                        pos.x = number(-7, 7) * 20;
                        pos.y = number(-7, 7) * 20;
                        pos.x += x();
                        pos.y += y();

                        metin2::log::debug("Item {} dropped at {:d} {:d} by {}", item->name(), pos.x, pos.y, name());
                    }
                }
            }
        }

        map_damage_.clear();
    }

    struct item_drop_penalty {
        int inventory_pct;
        int inventory_qty;
        int equipment_pct;
        int equipment_qty;
    };

    item_drop_penalty item_drop_penalty_kor[9] =
        {
            {0,   0, 0,  0},
            {0,   0, 0,  0},
            {0,   0, 0,  0},
            {0,   0, 0,  0},
            {0,   0, 0,  0},
            {25,  1, 5,  1},
            {50,  2, 10, 1},
            {75,  4, 15, 1},
            {100, 8, 20, 1},
    };

    void character::item_drop_penalty(character *killer) {
        if (my_shop())
            return;

        if (level() < 50)
            return;

        if (get_battle_arena().is_battle_arena_map(map_index()) == true) {
            return;
        }

        struct game::characters::item_drop_penalty *table = &item_drop_penalty_kor[0];

        if (level() < 10)
            return;

        int align_index;

        if (real_alignment() >= 120000)
            align_index = 0;
        else if (real_alignment() >= 80000)
            align_index = 1;
        else if (real_alignment() >= 40000)
            align_index = 2;
        else if (real_alignment() >= 10000)
            align_index = 3;
        else if (real_alignment() >= 0)
            align_index = 4;
        else if (real_alignment() > -40000)
            align_index = 5;
        else if (real_alignment() > -80000)
            align_index = 6;
        else if (real_alignment() > -120000)
            align_index = 7;
        else
            align_index = 8;

        std::vector<std::pair<item *, int>> vec_item;
        item *item_value;
        int i;
        bool is_drop_all_equipments = false;

        game::characters::item_drop_penalty &r = table[align_index];
        metin2::log::debug("{}: PK drop penalty for alignment index {:d}, inventory {:d}%, equipment {:d}%", name(), align_index, r.inventory_pct, r.equipment_pct);

        bool drop_inventory = r.inventory_pct >= number(1, 1000);
        bool drop_equipment = r.equipment_pct >= number(1, 100);
        bool drop_anti_drop_unique_item = false;

        if ((drop_inventory || drop_equipment) && is_equip_unique_item(UNIQUE_ITEM_SKIP_ITEM_DROP_PENALTY)) {
            drop_inventory = false;
            drop_equipment = false;
            drop_anti_drop_unique_item = true;
        }

        if (drop_inventory) {
            std::vector<uint8_t> vec_b_slots;

            for (i = 0; i < INVENTORY_MAX_NUM; ++i)
                if (inventory_item(i))
                    vec_b_slots.push_back(i);

            if (!vec_b_slots.empty()) {
                std::ranges::shuffle(vec_b_slots, random_engine());

                int qty = std::min<int>(vec_b_slots.size(), r.inventory_qty);

                if (qty)
                    qty = number(1, qty);

                for (i = 0; i < qty; ++i) {
                    item_value = inventory_item(vec_b_slots[i]);

                    if (is_set(item_value->anti_flag(), ITEM_ANTIFLAG_GIVE | ITEM_ANTIFLAG_PKDROP))
                        continue;

                    sync_quickslot(QUICKSLOT_TYPE_ITEM, vec_b_slots[i], 255);
                    vec_item.push_back(std::make_pair(item_value->remove_from_character(), INVENTORY));
                }
            } else if (align_index == 8)
                is_drop_all_equipments = true;
        }

        if (drop_equipment) {
            std::vector<uint8_t> vec_b_slots;

            for (i = 0; i < WEAR_MAX_NUM; ++i)
                if (wear(i))
                    vec_b_slots.push_back(i);

            if (!vec_b_slots.empty()) {
                std::ranges::shuffle(vec_b_slots, random_engine());
                int qty;

                if (is_drop_all_equipments)
                    qty = vec_b_slots.size();
                else
                    qty = std::min<int>(vec_b_slots.size(), number(1, r.equipment_qty));

                if (qty)
                    qty = number(1, qty);

                for (i = 0; i < qty; ++i) {
                    item_value = wear(vec_b_slots[i]);

                    if (is_set(item_value->anti_flag(), ITEM_ANTIFLAG_GIVE | ITEM_ANTIFLAG_PKDROP))
                        continue;

                    sync_quickslot(QUICKSLOT_TYPE_ITEM, vec_b_slots[i], 255);
                    vec_item.push_back(std::make_pair(item_value->remove_from_character(), EQUIPMENT));
                }
            }
        }

        if (drop_anti_drop_unique_item) {
            item *item_value;

            item_value = wear(WEAR_UNIQUE1);

            if (item_value && item_value->vnum() == UNIQUE_ITEM_SKIP_ITEM_DROP_PENALTY) {
                sync_quickslot(QUICKSLOT_TYPE_ITEM, WEAR_UNIQUE1, 255);
                vec_item.push_back(std::make_pair(item_value->remove_from_character(), EQUIPMENT));
            }

            item_value = wear(WEAR_UNIQUE2);

            if (item_value && item_value->vnum() == UNIQUE_ITEM_SKIP_ITEM_DROP_PENALTY) {
                sync_quickslot(QUICKSLOT_TYPE_ITEM, WEAR_UNIQUE2, 255);
                vec_item.push_back(std::make_pair(item_value->remove_from_character(), EQUIPMENT));
            }
        }

        {
            pixel_position pos;
            pos.x = x();
            pos.y = y();

            unsigned int i;

            for (i = 0; i < vec_item.size(); ++i) {
                item *item = vec_item[i].first;
                int window = vec_item[i].second;

                item->add_to_ground(map_index(), pos);
                item->start_destroy_event();

                metin2::log::info("Item {} dropped at {:d} {:d} on death of {}", item->name(), pos.x, pos.y, name());
                get_log_manager().item_log(this, item, "DEAD_DROP", (window == INVENTORY) ? "INVENTORY" : ((window == EQUIPMENT) ? "EQUIPMENT" : ""));

                pos.x = x() + number(-7, 7) * 20;
                pos.y = y() + number(-7, 7) * 20;
            }
        }
    }

    void character::dead(character *killer, bool immediate_dead) {
        if (is_dead())
            return;

        {
            if (is_horse_riding()) {
                stop_riding();
            } else if (get_mount_vnum()) {
                remove_affect(AFFECT_MOUNT_BONUS);
                mount_vnum_ = 0;
                un_equip_special_ride_unique_item();

                update_packet();
            }
        }

        if (!killer && killer_pid_)
            killer = get_character_manager().find_by_pid(killer_pid_);

        killer_pid_ = 0; // Must be reset here, or a later death is credited to this killer.

        bool is_agreed_pvp = false;
        bool is_under_guild_war = false;
        bool is_duel = false;
        bool is_forked = false;

        if (killer && killer->is_pc()) {
            if (killer->chr_target_ == this)
                killer->set_target(nullptr);

            if (!is_pc() && killer->get_dungeon())
                killer->get_dungeon()->inc_kill_count(killer, this);

            is_agreed_pvp = get_pvp_manager().dead(this, killer->player_id());
            is_duel = get_arena_manager().on_dead(killer, this);

            if (is_pc()) {
                guild *g1 = get_guild();
                guild *g2 = killer->get_guild();

                if (g1 && g2)
                    if (g1->under_war(g2->id()))
                        is_under_guild_war = true;

                killer->set_quest_npcid(get_vid());
                quest::get_quest_manager().kill(killer->player_id(), quest::QUEST_NO_NPC);
                get_guild_manager().kill(killer, this);
            }
        }

        if (is_pc()) {
            if (get_three_way_war().is_three_way_war_map_index(map_index()))
                is_forked = true;
        }

        if (killer &&
            !is_agreed_pvp &&
            !is_under_guild_war &&
            is_pc() &&
            !is_duel &&
            !is_forked &&
            !is_castle_map(map_index())) {
            if (gm_level() == GM_PLAYER || metin2::config::test_server()) {
                item_drop_penalty(killer);
            }
        }

        if (is_castle_map(map_index())) {
            if (CASTLE_FROG_VNUM == race_num())
                castle_frog_die(this, killer);
            else if (castle_is_guard_vnum(race_num()))
                castle_guard_die(this, killer);
            else if (castle_is_tower_vnum(race_num()))
                castle_tower_die(this, killer);
        }

        if (true == is_forked) {
            get_three_way_war().on_dead(this, killer);
        }

        set_position(POS_DEAD);
        clear_affect(true);

        if (killer && is_pc()) {
            if (!killer->is_pc()) {
                if (!is_forked) {
                    metin2::log::debug("{} ({}) died with penalty", name(), static_cast<const void *>(this));
                    set_bit(points_instant_.instant_flag, INSTANT_FLAG_DEATH_PENALTY);
                    get_log_manager().char_log(this, killer->race_num(), "DEAD_BY_NPC", killer->name());
                }
            } else {
                metin2::log::debug("{} ({}) killed by player {} ({})", name(), static_cast<const void *>(this), killer->name(), static_cast<const void *>(killer));
                remove_bit(points_instant_.instant_flag, INSTANT_FLAG_DEATH_PENALTY);

                if (empire() != killer->empire()) {
                    int ep = std::min(point(POINT_EMPIRE_POINT), killer->point(POINT_EMPIRE_POINT));

                    point_change(POINT_EMPIRE_POINT, -(ep / 10));
                    killer->point_change(POINT_EMPIRE_POINT, ep / 5);

                    if (point(POINT_EMPIRE_POINT) < 10) {
                    }

                    const std::string buf = std::format("{} {} {} {} {} {} {} {}",
                        empire(), alignment(), pk_mode(), name(),
                        killer->empire(), killer->alignment(), killer->pk_mode(), killer->name());

                    get_log_manager().char_log(this, killer->player_id(), "DEAD_BY_PC", buf.c_str());
                } else {
                    if (!is_agreed_pvp && !is_under_guild_war && !is_killer_mode() && alignment() >= 0 && !is_duel && !is_forked) {
                        int no_penalty_prob = 0;

                        if (killer->alignment() >= 0)
                            no_penalty_prob = 33;
                        else
                            no_penalty_prob = 20;

                        if (number(1, 100) < no_penalty_prob)
                            killer->chat_packet(CHAT_TYPE_INFO, lc_text("용신의 보호로 아이템이 떨어지지 않았습니다."));
                        else {
                            if (killer->get_party()) {
                                const int amount = -20000;
                                const int killer_x = killer->x();
                                const int killer_y = killer->y();
                                int count = 0;
                                auto is_near = [&](character *chr) { return distance_approx(chr->x() - killer_x, chr->y() - killer_y) < PARTY_DEFAULT_RANGE; };

                                killer->get_party()->for_each_online_member([&](character *chr) {
                                    if (is_near(chr))
                                        ++count;
                                });

                                if (count == 0)
                                    killer->update_alignment(-20000);
                                else {
                                    metin2::log::debug("Alignment shared in party: count {:d}, amount {:d}", count, amount);

                                    killer->get_party()->for_each_online_member([&](character *chr) {
                                        if (is_near(chr))
                                            chr->update_alignment(amount / count);
                                    });
                                }
                            } else
                                killer->update_alignment(-20000);
                        }
                    }

                    const std::string buf = std::format("{} {} {} {} {} {} {} {}",
                        empire(), alignment(), pk_mode(), name(),
                        killer->empire(), killer->alignment(), killer->pk_mode(), killer->name());

                    get_log_manager().char_log(this, killer->player_id(), "DEAD_BY_PC", buf.c_str());
                }
            }
        } else {
            metin2::log::debug("{} ({}) died", name(), static_cast<const void *>(this));
            remove_bit(points_instant_.instant_flag, INSTANT_FLAG_DEATH_PENALTY);
        }

        clear_sync();

        event_cancel(&stun_event);

        if (is_pc()) {
            last_dead_time_ = get_dword_time();
            set_killer_mode(false);
            get_desc()->set_phase(PHASE_DEAD);
        } else {
            if (!is_set(points_instant_.instant_flag, INSTANT_FLAG_NO_REWARD)) {
                if (!(killer && killer->is_pc() && killer->get_guild() && killer->get_guild()->under_any_war(GUILD_WAR_TYPE_FIELD))) {
                    if (get_mob_table().resurrection_vnum) {
                        character *ch_resurrect = get_character_manager().spawn_mob(get_mob_table().resurrection_vnum, map_index(), x(), y(), z(), true, (int)rotation());
                        if (get_dungeon() && ch_resurrect) {
                            ch_resurrect->set_dungeon(get_dungeon());
                        }

                        reward(false);
                    } else if (is_revive() == true) {
                        reward(false);
                    } else {
                        reward(true);
                    }
                } else {
                    if (killer->under_guild_war_info_message_time_ < get_dword_time()) {
                        killer->under_guild_war_info_message_time_ = get_dword_time() + 60000;
                        killer->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 길드전중에는 사냥에 따른 이익이 없습니다."));
                    }
                }
            }
        }

        if (get_mob_rank() >= MOB_RANK_BOSS && killer && killer->is_pc()) {
            const std::string buf = std::format("{} {}", metin2::config::server().channel, killer->map_index());
            if (is_stone())
                get_log_manager().char_log(killer, race_num(), "STONE_KILL", buf.c_str());
            else
                get_log_manager().char_log(killer, race_num(), "BOSS_KILL", buf.c_str());
        }

        packet_gc_dead pack;
        pack.header = HEADER_GC_DEAD;
        pack.vid = vid_;
        packet_around(&pack, sizeof(pack));

        remove_bit(points_instant_.instant_flag, INSTANT_FLAG_STUN);

        if (get_desc() != nullptr) {
            auto it = list_affect_.begin();

            while (it != list_affect_.end())
                send_affect_add_packet(get_desc(), *it++);
        }

        if (is_duel == false) {
            if (dead_event) {
                metin2::log::debug("{} ({}): dead event cancelled ({})", name(), static_cast<const void *>(this), static_cast<const void *>(dead_event.get()));
                event_cancel(&dead_event);
            }

            if (is_stone())
                clear_stone();

            if (get_dungeon()) {
                get_dungeon()->dead_character(this);
            }

            auto *event_info = alloc_event_info<char_dead_event_info>();

            if (is_pc()) {
                event_info->is_pc = true;
                event_info->id = this->player_id();

                dead_event = event_create(game::characters::dead_event, event_info, passes_per_sec(180));
            } else {
                event_info->is_pc = false;
                event_info->id = this->get_vid();

                if (is_revive() == false && has_reviver_in_party() == true) {
                    dead_event = event_create(game::characters::dead_event, event_info, immediate_dead ? 1 : passes_per_sec(3));
                } else {
                    dead_event = event_create(game::characters::dead_event, event_info, immediate_dead ? 1 : passes_per_sec(10));
                }
            }

            metin2::log::debug("{} ({}): dead event created ({})", name(), static_cast<const void *>(this), static_cast<const void *>(dead_event.get()));
        }

        if (exchange_ != nullptr) {
            exchange_->cancel();
        }

        if (is_cube_open() == true) {
            cube_close(this);
        }

        get_shop_manager().stop_shopping(this);
        close_my_shop();
        close_safebox();

        if (true == is_monster() && 2493 == get_mob_table().vnum) {
            if (nullptr != killer && nullptr != killer->get_guild()) {
                get_dragon_lair_manager().on_dragon_dead(this, killer->get_guild()->id());
            } else {
                metin2::log::error("Dragon lair: dragon died without a killer");
            }
        }
    }

    void character::set_last_attacked(uint32_t time) {
        assert(mob_inst != nullptr);

        mob_inst->last_attacked_time = time;
        mob_inst->pos_last_attacked = xyz();
    }

    void character::send_damage_packet(character *attacker, int damage_value, uint8_t damage_flag) {
        if (is_pc() == true || (attacker->is_pc() == true && attacker->target() == this)) {
            packet_gc_damage_info damage_info;
            memset(&damage_info, 0, sizeof(packet_gc_damage_info));

            damage_info.header = HEADER_GC_DAMAGE_INFO;
            damage_info.vid = (uint32_t)get_vid();
            damage_info.flag = damage_flag;
            damage_info.damage = damage_value;

            if (get_desc() != nullptr) {
                get_desc()->packet(&damage_info, sizeof(packet_gc_damage_info));
            }

            if (attacker->get_desc() != nullptr) {
                attacker->get_desc()->packet(&damage_info, sizeof(packet_gc_damage_info));
            }
        }
    }

    auto character::damage(character *attacker, int dam, damage_type type) -> bool {
        if (DAMAGE_TYPE_MAGIC == type) {
            dam = (int)((float)dam * (100 + (attacker->point(POINT_MAGIC_ATT_BONUS_PER) + attacker->point(POINT_MELEE_MAGIC_ATT_BONUS_PER))) / 100.f + 0.5f);
        }
        if (race_num() == 5001) {
            bool drop_money = false;
            int percent_value = (hp() * 100) / max_hp();

            if (percent_value <= 10 && max_sp() < 5) {
                set_max_sp(5);
                drop_money = true;
            } else if (percent_value <= 20 && max_sp() < 4) {
                set_max_sp(4);
                drop_money = true;
            } else if (percent_value <= 40 && max_sp() < 3) {
                set_max_sp(3);
                drop_money = true;
            } else if (percent_value <= 60 && max_sp() < 2) {
                set_max_sp(2);
                drop_money = true;
            } else if (percent_value <= 80 && max_sp() < 1) {
                set_max_sp(1);
                drop_money = true;
            }

            if (drop_money) {
                uint32_t gold_value = 1000;
                int split_count = number(10, 13);

                metin2::log::debug("Waegu drops gold on hit {:d} times", max_sp());

                for (int i = 1; i <= split_count; ++i) {
                    pixel_position pos;
                    item *item;

                    if ((item = get_item_manager().create_item(1, gold_value / split_count))) {
                        if (i != 0) {
                            pos.x = (number(-14, 14) + number(-14, 14)) * 20;
                            pos.y = (number(-14, 14) + number(-14, 14)) * 20;

                            pos.x += x();
                            pos.y += y();
                        }

                        item->add_to_ground(map_index(), pos);
                        item->start_destroy_event();
                    }
                }
            }
        }

        if (type != DAMAGE_TYPE_NORMAL && type != DAMAGE_TYPE_NORMAL_RANGE) {
            if (is_affect_flag(AFF_TERROR)) {
                int pct = skill_power(SKILL_TERROR) / 400;

                if (prob(pct))
                    return false;
            }
        }

        int cur_hp = hp();
        int cur_sp = sp();

        bool is_critical = false;
        bool is_penetrate = false;
        bool is_death_blow = false;

        enum damage_flag {
            DAMAGE_NORMAL = (1 << 0),
            DAMAGE_POISON = (1 << 1),
            DAMAGE_DODGE = (1 << 2),
            DAMAGE_BLOCK = (1 << 3),
            DAMAGE_PENETRATE = (1 << 4),
            DAMAGE_CRITICAL = (1 << 5),
        };

        if (type == DAMAGE_TYPE_MELEE || type == DAMAGE_TYPE_RANGE || type == DAMAGE_TYPE_MAGIC) {
            if (attacker) {
                int critical_pct = attacker->point(POINT_CRITICAL_PCT);

                if (!is_pc())
                    critical_pct += attacker->marriage_bonus(UNIQUE_ITEM_MARRIAGE_CRITICAL_BONUS);

                if (critical_pct) {
                    if (critical_pct >= 10)
                        critical_pct = 5 + (critical_pct - 10) / 4;
                    else
                        critical_pct /= 2;

                    critical_pct -= point(POINT_RESIST_CRITICAL);

                    if (prob(critical_pct)) {
                        is_critical = true;
                        dam *= 2;
                        effect_packet(SE_CRITICAL);

                        if (is_affect_flag(AFF_MANASHIELD)) {
                            remove_affect(AFF_MANASHIELD);
                        }
                    }
                }

                int penetrate_pct = attacker->point(POINT_PENETRATE_PCT);

                if (!is_pc())
                    penetrate_pct += attacker->marriage_bonus(UNIQUE_ITEM_MARRIAGE_PENETRATE_BONUS);

                if (penetrate_pct) {
                    {
                        skill_proto *sk = get_skill_manager().get(SKILL_RESIST_PENETRATE);

                        if (nullptr != sk) {
                            sk->set_point_var("k", 1.0f * skill_power(SKILL_RESIST_PENETRATE) / 100.0f);

                            penetrate_pct -= static_cast<int>(sk->point_poly.eval());
                        }
                    }

                    if (penetrate_pct >= 10) {
                        penetrate_pct = 5 + (penetrate_pct - 10) / 4;
                    } else {
                        penetrate_pct /= 2;
                    }

                    penetrate_pct -= point(POINT_RESIST_PENETRATE);

                    if (prob(penetrate_pct)) {
                        is_penetrate = true;

                        if (metin2::config::test_server())
                            chat_packet(CHAT_TYPE_INFO, lc_text("관통 추가 데미지 %d"), point(POINT_DEF_GRADE) * (100 + point(POINT_DEF_BONUS)) / 100);

                        dam += point(POINT_DEF_GRADE) * (100 + point(POINT_DEF_BONUS)) / 100;

                        if (is_affect_flag(AFF_MANASHIELD)) {
                            remove_affect(AFF_MANASHIELD);
                        }
                    }
                }
            }
        } else if (type == DAMAGE_TYPE_NORMAL || type == DAMAGE_TYPE_NORMAL_RANGE) {
            if (type == DAMAGE_TYPE_NORMAL) {
                if (point(POINT_BLOCK) && prob(point(POINT_BLOCK))) {
                    if (metin2::config::test_server()) {
                        attacker->chat_packet(CHAT_TYPE_INFO, lc_text("%s 블럭! (%d%%)"), name(), point(POINT_BLOCK));
                        chat_packet(CHAT_TYPE_INFO, lc_text("%s 블럭! (%d%%)"), name(), point(POINT_BLOCK));
                    }

                    send_damage_packet(attacker, 0, DAMAGE_BLOCK);
                    return false;
                }
            } else if (type == DAMAGE_TYPE_NORMAL_RANGE) {
                if (point(POINT_DODGE) && prob(point(POINT_DODGE))) {
                    if (metin2::config::test_server()) {
                        attacker->chat_packet(CHAT_TYPE_INFO, lc_text("%s 회피! (%d%%)"), name(), point(POINT_DODGE));
                        chat_packet(CHAT_TYPE_INFO, lc_text("%s 회피! (%d%%)"), name(), point(POINT_DODGE));
                    }

                    send_damage_packet(attacker, 0, DAMAGE_DODGE);
                    return false;
                }
            }

            if (is_affect_flag(AFF_JEONGWIHON))
                dam = (int)(dam * (100 + skill_power(SKILL_JEONGWI) * 25 / 100) / 100);

            if (is_affect_flag(AFF_TERROR))
                dam = (int)(dam * (95 - skill_power(SKILL_TERROR) / 5) / 100);

            if (is_affect_flag(AFF_HOSIN))
                dam = dam * (100 - point(POINT_RESIST_NORMAL_DAMAGE)) / 100;

            if (attacker) {
                if (type == DAMAGE_TYPE_NORMAL) {
                    if (point(POINT_REFLECT_MELEE)) {
                        int reflect_damage = dam * point(POINT_REFLECT_MELEE) / 100;

                        if (attacker->is_immune(IMMUNE_REFLECT))
                            reflect_damage = int(reflect_damage / 3.0f + 0.5f);

                        attacker->damage(this, reflect_damage, DAMAGE_TYPE_SPECIAL);
                    }
                }

                int critical_pct = attacker->point(POINT_CRITICAL_PCT);

                if (!is_pc())
                    critical_pct += attacker->marriage_bonus(UNIQUE_ITEM_MARRIAGE_CRITICAL_BONUS);

                if (critical_pct) {
                    critical_pct -= point(POINT_RESIST_CRITICAL);

                    if (prob(critical_pct)) {
                        is_critical = true;
                        dam *= 2;
                        effect_packet(SE_CRITICAL);
                    }
                }

                int penetrate_pct = attacker->point(POINT_PENETRATE_PCT);

                if (!is_pc())
                    penetrate_pct += attacker->marriage_bonus(UNIQUE_ITEM_MARRIAGE_PENETRATE_BONUS);

                {
                    skill_proto *sk = get_skill_manager().get(SKILL_RESIST_PENETRATE);

                    if (nullptr != sk) {
                        sk->set_point_var("k", 1.0f * skill_power(SKILL_RESIST_PENETRATE) / 100.0f);

                        penetrate_pct -= static_cast<int>(sk->point_poly.eval());
                    }
                }

                if (penetrate_pct) {

                    penetrate_pct -= point(POINT_RESIST_PENETRATE);

                    if (prob(penetrate_pct)) {
                        is_penetrate = true;

                        if (metin2::config::test_server())
                            chat_packet(CHAT_TYPE_INFO, lc_text("관통 추가 데미지 %d"), point(POINT_DEF_GRADE) * (100 + point(POINT_DEF_BONUS)) / 100);
                        dam += point(POINT_DEF_GRADE) * (100 + point(POINT_DEF_BONUS)) / 100;
                    }
                }

                if (attacker->point(POINT_STEAL_HP)) {
                    int pct = 1;

                    if (number(1, 10) <= pct) {
                        int hp_value = std::min(dam, std::max(0, cur_hp)) * attacker->point(POINT_STEAL_HP) / 100;

                        if (hp_value > 0 && hp() >= hp_value) {
                            create_fly(FLY_HP_SMALL, attacker);
                            attacker->point_change(POINT_HP, hp_value);
                            point_change(POINT_HP, -hp_value);
                        }
                    }
                }

                if (attacker->point(POINT_STEAL_SP)) {
                    int pct = 1;

                    if (number(1, 10) <= pct) {
                        int cur;

                        if (is_pc())
                            cur = cur_sp;
                        else
                            cur = cur_hp;

                        int sp_value = std::min(dam, std::max(0, cur)) * attacker->point(POINT_STEAL_SP) / 100;

                        if (sp_value > 0 && cur >= sp_value) {
                            create_fly(FLY_SP_SMALL, attacker);
                            attacker->point_change(POINT_SP, sp_value);

                            if (is_pc())
                                point_change(POINT_SP, -sp_value);
                        }
                    }
                }

                if (attacker->point(POINT_STEAL_GOLD)) {
                    if (prob(attacker->point(POINT_STEAL_GOLD))) {
                        int amount = number(1, level());
                        attacker->point_change(POINT_GOLD, amount);
                        get_db_manager().send_money_log(MONEY_LOG_MISC, 1, amount);
                    }
                }

                if (attacker->point(POINT_HIT_HP_RECOVERY) && number(0, 4) > 0) {
                    int i = ((cur_hp >= 0) ? std::min(dam, cur_hp) : dam) * attacker->point(POINT_HIT_HP_RECOVERY) / 100;

                    if (i) {
                        create_fly(FLY_HP_SMALL, attacker);
                        attacker->point_change(POINT_HP, i);
                    }
                }

                if (attacker->point(POINT_HIT_SP_RECOVERY) && number(0, 4) > 0) {
                    int i = ((cur_hp >= 0) ? std::min(dam, cur_hp) : dam) * attacker->point(POINT_HIT_SP_RECOVERY) / 100;

                    if (i) {
                        create_fly(FLY_SP_SMALL, attacker);
                        attacker->point_change(POINT_SP, i);
                    }
                }

                if (attacker->point(POINT_MANA_BURN_PCT)) {
                    if (prob(attacker->point(POINT_MANA_BURN_PCT)))
                        point_change(POINT_SP, -50);
                }
            }
        }

        switch (type) {
        case DAMAGE_TYPE_NORMAL:
        case DAMAGE_TYPE_NORMAL_RANGE:
            if (attacker)
                if (attacker->point(POINT_NORMAL_HIT_DAMAGE_BONUS))
                    dam = dam * (100 + attacker->point(POINT_NORMAL_HIT_DAMAGE_BONUS)) / 100;

            dam = dam * (100 - std::min(99, point(POINT_NORMAL_HIT_DEFEND_BONUS))) / 100;
            break;

        case DAMAGE_TYPE_MELEE:
        case DAMAGE_TYPE_RANGE:
        case DAMAGE_TYPE_FIRE:
        case DAMAGE_TYPE_ICE:
        case DAMAGE_TYPE_ELEC:
        case DAMAGE_TYPE_MAGIC:
            if (attacker)
                if (attacker->point(POINT_SKILL_DAMAGE_BONUS))
                    dam = dam * (100 + attacker->point(POINT_SKILL_DAMAGE_BONUS)) / 100;

            dam = dam * (100 - std::min(99, point(POINT_SKILL_DEFEND_BONUS))) / 100;
            break;

        default:
            break;
        }

        if (is_affect_flag(AFF_MANASHIELD)) {
            int damage_sp_part = dam / 3;
            int damage_to_sp = damage_sp_part * point(POINT_MANASHIELD) / 100;
            int sp_value = sp();

            if (damage_to_sp <= sp_value) {
                point_change(POINT_SP, -damage_to_sp);
                dam -= damage_sp_part;
            } else {
                point_change(POINT_SP, -sp());
                dam -= sp_value * 100 / std::max(point(POINT_MANASHIELD), 1);
            }
        }

        if (point(POINT_MALL_DEFBONUS) > 0) {
            int dec_dam = std::min(200, dam * point(POINT_MALL_DEFBONUS) / 100);
            dam -= dec_dam;
        }

        if (attacker) {
            if (attacker->point(POINT_MALL_ATTBONUS) > 0) {
                int add_dam = std::min(300, dam * attacker->limit_point(POINT_MALL_ATTBONUS) / 100);
                dam += add_dam;
            }

            int empire_value = empire();
            int32_t map_index_value = map_index();
            int map_empire = get_sectree_manager().empire_from_map_index(map_index_value);

            if (attacker->is_pc()) {
                empire_value = attacker->empire();
                map_index_value = attacker->map_index();
                map_empire = get_sectree_manager().empire_from_map_index(map_index_value);

                if (empire_value && map_empire && empire_value != map_empire) {
                    int percent = 10;

                    if (184 <= map_index_value && map_index_value <= 189) {
                        percent = 9;
                    } else {
                        percent = 9;
                    }

                    dam = dam * percent / 10;
                }

                if (!is_pc() && monster_drain_sp_point()) {
                    int drain = monster_drain_sp_point();

                    if (drain <= attacker->sp())
                        attacker->point_change(POINT_SP, -drain);
                    else {
                        int sp_value = attacker->sp();
                        attacker->point_change(POINT_SP, -sp_value);
                    }
                }

            } else if (attacker->is_guard_npc()) {
                set_bit(points_instant_.instant_flag, INSTANT_FLAG_NO_REWARD);
                stun();
                return true;
            }

            if (attacker->is_pc() && get_monarch().is_power_up(attacker->empire())) {
                dam += dam / 10;
            }

            if (is_pc() && get_monarch().is_defence_up(empire())) {
                dam -= dam / 10;
            }
        }

        if (!get_sectree() || get_sectree()->is_attr(x(), y(), ATTR_BANPK))
            return false;

        if (!is_pc()) {
            if (party_ && party_->leader())
                party_->leader()->set_last_attacked(get_dword_time());
            else
                set_last_attacked(get_dword_time());

            monster_chat(MONSTER_CHAT_ATTACKED);
        }

        if (is_stun()) {
            dead(attacker);
            return true;
        }

        if (is_dead())
            return true;

        if (type == DAMAGE_TYPE_POISON) {
            if (hp() - dam <= 0) {
                dam = hp() - 1;
            }
        }

        if (is_monster() && is_stone_skinner()) {
            if (hp_pct() < get_mob_table().stone_skin_point)
                dam /= 2;
        }

        if (attacker) {
            if (attacker->is_monster() && attacker->is_death_blower()) {
                if (attacker->is_death_blow()) {
                    if (number(JOB_WARRIOR, JOB_MAX_NUM - 1) == job()) {
                        is_death_blow = true;
                        dam = dam * 4;
                    }
                }
            }

            dam = blue_dragon_damage(this, attacker, dam);

            uint8_t damage_flag = 0;

            if (type == DAMAGE_TYPE_POISON)
                damage_flag = DAMAGE_POISON;
            else
                damage_flag = DAMAGE_NORMAL;

            if (is_critical == true)
                damage_flag |= DAMAGE_CRITICAL;

            if (is_penetrate == true)
                damage_flag |= DAMAGE_PENETRATE;

            float dam_mul = this->dam_mul();
            float temp_dam = dam;
            dam = temp_dam * dam_mul + 0.5f;

            if (attacker)
                send_damage_packet(attacker, dam, damage_flag);

            if (metin2::config::test_server()) {
                if (attacker) {
                    attacker->chat_packet(CHAT_TYPE_INFO, "-> %s, DAM %d HP %d(%d%%) %s%s",
                        name(),
                        dam,
                        hp(),
                        (hp() * 100) / max_hp(),
                        is_critical ? "crit " : "",
                        is_penetrate ? "pene " : "",
                        is_death_blow ? "deathblow " : "");
                }

                chat_packet(CHAT_TYPE_PARTY, "<- %s, DAM %d HP %d(%d%%) %s%s",
                    attacker ? attacker->name() : nullptr,
                    dam,
                    hp(),
                    (hp() * 100) / max_hp(),
                    is_critical ? "crit " : "",
                    is_penetrate ? "pene " : "",
                    is_death_blow ? "deathblow " : "");
            }

            if (detail_log_) {
                chat_packet(CHAT_TYPE_INFO, lc_text("%s[%d]가 공격 위치: %d %d"), attacker->name(), (uint32_t)attacker->get_vid(), attacker->x(), attacker->y());
            }
        }

        if (!cannot_dead_) {
            point_change(POINT_HP, -dam, false);
        }

        if (attacker && dam > 0 && is_npc()) {
            auto it = map_damage_.find(attacker->get_vid());

            if (it == map_damage_.end()) {
                map_damage_.insert(damage_map::value_type(attacker->get_vid(), battle_info(dam, 0)));
                it = map_damage_.find(attacker->get_vid());
            } else {
                it->second.total_damage += dam;
            }

            start_recovery_event();

            update_aggr_point_ex(attacker, type, dam, it->second);
        }

        if (hp() <= 0) {
            stun();

            if (attacker && !attacker->is_npc())
                killer_pid_ = attacker->player_id();
            else
                killer_pid_ = 0;
        }

        return false;
    }

    void character::distribute_hp(character *killer) {
        if (killer->get_dungeon())
            return;
    }

    static void give_exp(character *from, character *to, int exp) {
        exp = calculate_value_lvdelta(to->level(), from->level(), exp);

        if (metin2::config::game().distribution_test_server)
            exp *= 3;

        int base_exp = exp;

        exp = exp * (100 + get_priv_manager().priv(to, PRIV_EXP_PCT)) / 100;

        {
            if (to->is_equip_unique_item(UNIQUE_ITEM_LARBOR_MEDAL))
                exp += exp * 20 / 100;

            if (to->map_index() >= 660000 && to->map_index() < 670000)
                exp += exp * 20 / 100;

            if (to->point(POINT_EXP_DOUBLE_BONUS))
                if (prob(to->point(POINT_EXP_DOUBLE_BONUS)))
                    exp += exp * 30 / 100;

            if (to->is_equip_unique_item(UNIQUE_ITEM_DOUBLE_EXP))
                exp += exp * 50 / 100;

            switch (to->get_mount_vnum()) {
            case 20110:
            case 20111:
            case 20112:
            case 20113:
                if (to->is_equip_unique_item(71115) || to->is_equip_unique_item(71117) || to->is_equip_unique_item(71119) ||
                    to->is_equip_unique_item(71121)) {
                    exp += exp * 10 / 100;
                }
                break;

            case 20114:
            case 20120:
            case 20121:
            case 20122:
            case 20123:
            case 20124:
            case 20125:
                exp += exp * 30 / 100;
                break;
            }
        }

        if (to->premium_remain_seconds(PREMIUM_EXP) > 0) {
            exp += (exp * 50 / 100);
        }

        if (to->is_equip_unique_group(UNIQUE_GROUP_RING_OF_EXP) == true) {
            exp += (exp * 50 / 100);
        }

        exp += exp * to->marriage_bonus(UNIQUE_ITEM_MARRIAGE_EXP_BONUS) / 100;

        exp += (exp * to->point(POINT_RAMADAN_CANDY_BONUS_EXP) / 100);
        exp += (exp * to->point(POINT_MALL_EXPBONUS) / 100);
        exp += (exp * to->point(POINT_EXP) / 100);

        if (metin2::config::test_server()) {
            metin2::log::debug("Bonus exp: ramadan candy {:d}, mall {:d}, point {:d}",
                to->point(POINT_RAMADAN_CANDY_BONUS_EXP),
                to->point(POINT_MALL_EXPBONUS),
                to->point(POINT_EXP));
        }

        exp = exp * get_character_manager().mob_exp_rate(to) / 100;

        exp = std::min<int>(to->next_exp() / 10, exp);

        if (metin2::config::test_server()) {
            if (quest::get_quest_manager().event_flag("exp_bonus_log") && base_exp > 0)
                to->chat_packet(CHAT_TYPE_INFO, "exp bonus %d%%", (exp - base_exp) * 100 / base_exp);
        }

        exp = adjust_exp_by_level(to, exp);

        to->point_change(POINT_EXP, exp, true);
        from->create_fly(FLY_EXP, to);

        {
            character *you = to->marry_partner();
            if (you) {
                uint32_t update_point = 2000 * exp / to->level() / to->level() / 3;

                if (to->premium_remain_seconds(PREMIUM_MARRIAGE_FAST) > 0 ||
                    you->premium_remain_seconds(PREMIUM_MARRIAGE_FAST) > 0)
                    update_point = (uint32_t)(update_point * 3);

                marriage::marriage *marriage = marriage::get_manager().get(to->player_id());

                if (marriage && marriage->is_near())
                    marriage->update(update_point);
            }
        }
    }

    struct damage_info {
        int dam;
        character *attacker;
        game::parties::party *party;

        void clear() {
            attacker = nullptr;
            party = nullptr;
        }

        inline void distribute(character *ch, int exp) {
            if (attacker)
                give_exp(ch, attacker, exp);
            else if (party) {
                const int x = ch->x();
                const int y = ch->y();
                int total = 0;
                int member_count = 0;

                party->for_each_online_member([&](character *member) {
                    if (distance_approx(member->x() - x, member->y() - y) <= PARTY_DEFAULT_RANGE) {
                        total += party_exp_distribute_table[member->level()];

                        ++member_count;
                    }
                });

                if (party->is_position_near_leader(ch))
                    exp = exp * (100 + party->exp_bonus_percent()) / 100;

                if (metin2::config::test_server()) {
                    if (quest::get_quest_manager().event_flag("exp_bonus_log") && party->exp_bonus_percent())
                        party->chat_packet_to_all_member(CHAT_TYPE_INFO, "exp party bonus %d%%", party->exp_bonus_percent());
                }

                if (party->exp_centralize_character()) {
                    character *tch = party->exp_centralize_character();

                    if (distance_approx(ch->x() - tch->x(), ch->y() - tch->y()) <= PARTY_DEFAULT_RANGE) {
                        int exp_centeralize = (int)(exp * 0.05f);
                        exp -= exp_centeralize;

                        give_exp(ch, party->exp_centralize_character(), exp_centeralize);
                    }
                }

                const uint32_t i_exp = exp;
                const int mode = party->exp_distribution_mode();

                if (member_count == 0)
                    member_count = 1;

                party->for_each_online_member([&](character *member) {
                    if (distance_approx(member->x() - x, member->y() - y) <= PARTY_DEFAULT_RANGE) {
                        uint32_t exp2 = 0;

                        switch (mode) {
                        case PARTY_EXP_DISTRIBUTION_NON_PARITY:
                            exp2 = (uint32_t)(i_exp * (float)party_exp_distribute_table[member->level()] / total);
                            break;

                        case PARTY_EXP_DISTRIBUTION_PARITY:
                            exp2 = i_exp / member_count;
                            break;

                        default:
                            metin2::log::error("Unknown party exp distribution mode {:d}", mode);
                            return;
                        }

                        give_exp(ch, member, exp2);
                    }
                });
            }
        }
    };

    auto character::distribute_exp() -> character * {
        int exp_to_distribute = get_exp();

        if (exp_to_distribute <= 0)
            return nullptr;

        int total_dam = 0;
        character *chr_most_attacked = nullptr;
        int most_dam = 0;

        using damage_info_list = std::vector<damage_info>;
        damage_info_list damage_info_table;
        std::map<party *, damage_info> map_party_damage;

        damage_info_table.reserve(map_damage_.size());

        auto it = map_damage_.begin();

        while (it != map_damage_.end()) {
            const vid &vid = it->first;
            int dam = it->second.total_damage;

            ++it;

            character *attacker = get_character_manager().find(vid);

            if (!attacker || attacker->is_npc() || distance_approx(x() - attacker->x(), y() - attacker->y()) > 5000)
                continue;

            total_dam += dam;
            if (!chr_most_attacked || dam > most_dam) {
                chr_most_attacked = attacker;
                most_dam = dam;
            }

            if (attacker->get_party()) {
                auto it = map_party_damage.find(attacker->get_party());
                if (it == map_party_damage.end()) {
                    damage_info di;
                    di.dam = dam;
                    di.attacker = nullptr;
                    di.party = attacker->get_party();
                    map_party_damage.insert(std::make_pair(di.party, di));
                } else {
                    it->second.dam += dam;
                }
            } else {
                damage_info di;

                di.dam = dam;
                di.attacker = attacker;
                di.party = nullptr;

                damage_info_table.push_back(di);
            }
        }

        for (auto it = map_party_damage.begin(); it != map_party_damage.end(); ++it) {
            damage_info_table.push_back(it->second);
        }

        set_exp(0);

        if (total_dam == 0)
            return nullptr;

        if (chr_stone_) {
            int exp_value = exp_to_distribute >> 1;
            chr_stone_->set_exp(chr_stone_->get_exp() + exp_value);
            exp_to_distribute -= exp_value;
        }

        metin2::log::debug("{}: distributing {:d} exp among {:d} attackers (total damage {:d})",
            name(), exp_to_distribute, damage_info_table.size(), total_dam);

        if (damage_info_table.empty())
            return nullptr;

        distribute_hp(chr_most_attacked);

        {
            auto di = damage_info_table.begin();
            {
                damage_info_list::iterator it;

                for (it = damage_info_table.begin(); it != damage_info_table.end(); ++it) {
                    if (it->dam > di->dam)
                        di = it;
                }
            }

            int exp_value = exp_to_distribute / 5;
            exp_to_distribute -= exp_value;

            float percent = (float)di->dam / total_dam;

            if (percent > 1.0f) {
                metin2::log::error("Exp share {:f} over 1.0 for {}", percent, di->attacker->name());
                percent = 1.0f;
            }

            exp_value += (int)(exp_to_distribute * percent);

            di->distribute(this, exp_value);

            if (percent == 1.0f)
                return chr_most_attacked;

            di->clear();
        }

        {
            damage_info_list::iterator it;

            for (it = damage_info_table.begin(); it != damage_info_table.end(); ++it) {
                damage_info &di = *it;

                float percent = (float)di.dam / total_dam;

                if (percent > 1.0f) {
                    metin2::log::error("Exp share {:f} over 1.0 for {}", percent, di.attacker->name());
                    percent = 1.0f;
                }

                di.distribute(this, (int)(exp_to_distribute * percent));
            }
        }

        return chr_most_attacked;
    }

    auto character::arrow_and_bow(item **ppk_bow, item **ppk_arrow, int arrow_count) -> int {
        item *bow;

        if (!(bow = wear(WEAR_WEAPON)) || bow->proto()->sub_type != WEAPON_BOW) {
            return 0;
        }

        item *arrow;

        if (!(arrow = wear(WEAR_ARROW)) || arrow->type() != ITEM_WEAPON ||
            arrow->proto()->sub_type != WEAPON_ARROW) {
            return 0;
        }

        arrow_count = std::min<int>(arrow_count, arrow->count());

        *ppk_bow = bow;
        *ppk_arrow = arrow;

        return arrow_count;
    }

    void character::use_arrow(item *arrow, uint32_t arrow_count) {
        int count = arrow->count();
        uint32_t vnum = arrow->vnum();
        count = count - std::min<int>(count, arrow_count);
        arrow->set_count(count);

        if (count == 0) {
            item *new_arrow = find_specify_item(vnum);

            metin2::log::debug("Arrow stack {:d} used up, next stack {}", vnum, static_cast<const void *>(new_arrow));

            if (new_arrow)
                equip_item(new_arrow);
        }
    }

    auto character::shoot(uint8_t type) -> bool {
        metin2::log::debug("{}: shoot type {:d}, {:d} fly targets", name(), type, vec_fly_targets_.size());

        if (!can_move()) {
            return false;
        }

        bool succeed = false;

        auto shoot_at = [&](uint32_t target_vid) {
            if (type > 1) {
                if (metin2::config::game().skill_disable)
                    return;

                skill_use_info_[type].set_main_target_vid(target_vid);
            }

            character *victim = get_character_manager().find(target_vid);

            if (!victim)
                return;

            if (!battle_is_attackable(this, victim))
                return;

            if (is_npc()) {
                if (distance_approx(x() - victim->x(), y() - victim->y()) > 5000)
                    return;
            }

            item *bow;
            item *arrow;

            switch (type) {
            case 0:
                {
                    int dam = 0;

                    if (is_pc()) {
                        if (job() != JOB_ASSASSIN)
                            return;

                        if (0 == arrow_and_bow(&bow, &arrow))
                            return;

                        if (skill_group() != 0)
                            if (!is_npc() && skill_group() != 2) {
                                if (sp() < 5)
                                    return;

                                point_change(POINT_SP, -5);
                            }

                        dam = calc_arrow_damage(this, victim, bow, arrow);
                        use_arrow(arrow, 1);

                        uint32_t current_time = get_dword_time();
                        if (is_speed_hack(this, victim, current_time))
                            dam = 0;
                    } else
                        dam = calc_melee_damage(this, victim);

                    normal_attack_affect(this, victim);

                    dam = dam * (100 - victim->point(POINT_RESIST_BOW)) / 100;

                    on_move(true);
                    victim->on_move();

                    if (victim->can_begin_fight())
                        victim->begin_fight(this);

                    victim->damage(this, dam, DAMAGE_TYPE_NORMAL_RANGE);
                }
                break;

            case 1:
                {
                    int dam;

                    if (is_pc())
                        return;

                    dam = calc_magic_damage(this, victim);

                    normal_attack_affect(this, victim);

                    dam = dam * (100 - victim->point(POINT_RESIST_MAGIC)) / 100;

                    on_move(true);
                    victim->on_move();

                    if (victim->can_begin_fight())
                        victim->begin_fight(this);

                    victim->damage(this, dam, DAMAGE_TYPE_MAGIC);
                }
                break;

            case SKILL_YEONSA:
                {
                    int use_arrow_value = 1;

                    {
                        if (use_arrow_value == arrow_and_bow(&bow, &arrow, use_arrow_value)) {
                            on_move(true);
                            victim->on_move();

                            if (victim->can_begin_fight())
                                victim->begin_fight(this);

                            compute_skill(type, victim);
                            use_arrow(arrow, use_arrow_value);

                            if (victim->is_dead())
                                break;

                        } else
                            break;
                    }
                }
                break;

            case SKILL_KWANKYEOK:
                {
                    int use_arrow_value = 1;

                    if (use_arrow_value == arrow_and_bow(&bow, &arrow, use_arrow_value)) {
                        on_move(true);
                        victim->on_move();

                        if (victim->can_begin_fight())
                            victim->begin_fight(this);

                        metin2::log::debug("{} hits {} with kwankeyok", name(), victim->name());
                        compute_skill(type, victim);
                        use_arrow(arrow, use_arrow_value);
                    }
                }
                break;

            case SKILL_GIGUNG:
                {
                    int use_arrow_value = 1;
                    if (use_arrow_value == arrow_and_bow(&bow, &arrow, use_arrow_value)) {
                        on_move(true);
                        victim->on_move();

                        if (victim->can_begin_fight())
                            victim->begin_fight(this);

                        metin2::log::debug("{} hits {} with gigung", name(), victim->name());
                        compute_skill(type, victim);
                        use_arrow(arrow, use_arrow_value);
                    }
                }

                break;
            case SKILL_HWAJO:
                {
                    int use_arrow_value = 1;
                    if (use_arrow_value == arrow_and_bow(&bow, &arrow, use_arrow_value)) {
                        on_move(true);
                        victim->on_move();

                        if (victim->can_begin_fight())
                            victim->begin_fight(this);

                        metin2::log::debug("{} hits {} with hwajo", name(), victim->name());
                        compute_skill(type, victim);
                        use_arrow(arrow, use_arrow_value);
                    }
                }

                break;

            case SKILL_HORSE_WILDATTACK_RANGE:
                {
                    int use_arrow_value = 1;
                    if (use_arrow_value == arrow_and_bow(&bow, &arrow, use_arrow_value)) {
                        on_move(true);
                        victim->on_move();

                        if (victim->can_begin_fight())
                            victim->begin_fight(this);

                        metin2::log::debug("{} hits {} with horse wild attack", name(), victim->name());
                        compute_skill(type, victim);
                        use_arrow(arrow, use_arrow_value);
                    }
                }

                break;

            case SKILL_MARYUNG:
            case SKILL_TUSOK:
            case SKILL_BIPABU:
            case SKILL_NOEJEON:
            case SKILL_GEOMPUNG:
            case SKILL_SANGONG:
            case SKILL_MAHWAN:
            case SKILL_PABEOB:
                {
                    on_move(true);
                    victim->on_move();

                    if (victim->can_begin_fight())
                        victim->begin_fight(this);

                    metin2::log::debug("{}: skill {:d} hits {}", name(), type, victim->name());
                    compute_skill(type, victim);
                }
                break;

            case SKILL_CHAIN:
                {
                    on_move(true);
                    victim->on_move();

                    if (victim->can_begin_fight())
                        victim->begin_fight(this);

                    metin2::log::debug("{}: skill {:d} hits {}", name(), type, victim->name());
                    compute_skill(type, victim);
                }
                break;

            case SKILL_YONGBI:
                {
                    on_move(true);
                }
                break;

            default:
                metin2::log::error("Unknown range attack type {:d}", (int)type);
                break;
            }

            succeed = true;
        };

        if (fly_target_id_ != 0) {
            shoot_at(fly_target_id_);
            fly_target_id_ = 0;
        }

        for (uint32_t target_vid : vec_fly_targets_)
            shoot_at(target_vid);
        vec_fly_targets_.clear();

        return succeed;
    }

    void character::fly_target(uint32_t target_vid_value, int32_t x, int32_t y, uint8_t header_value) {
        character *victim = get_character_manager().find(target_vid_value);
        packet_gc_fly_targeting pack;

        pack.header = (header_value == HEADER_CG_FLY_TARGETING) ? HEADER_GC_FLY_TARGETING : HEADER_GC_ADD_FLY_TARGETING;
        pack.shooter_vid = get_vid();

        if (victim) {
            pack.target_vid = victim->get_vid();
            pack.x = victim->x();
            pack.y = victim->y();

            if (header_value == HEADER_CG_FLY_TARGETING)
                fly_target_id_ = target_vid_value;
            else
                vec_fly_targets_.push_back(target_vid_value);
        } else {
            pack.target_vid = 0;
            pack.x = x;
            pack.y = y;
        }

        metin2::log::debug("{}: fly target vid {:d} at {:d} {:d}", name(), pack.target_vid, pack.x, pack.y);
        packet_around(&pack, sizeof(pack), this);
    }

    auto character::nearest_victim(character *chr) -> character * {
        if (nullptr == chr)
            chr = this;

        float min_dist = 99999.0f;
        character *victim = nullptr;

        auto it = map_damage_.begin();

        while (it != map_damage_.end()) {
            const vid &vid = it->first;
            ++it;

            character *attacker = get_character_manager().find(vid);

            if (!attacker)
                continue;

            if (attacker->is_affect_flag(AFF_EUNHYUNG) ||
                attacker->is_affect_flag(AFF_INVISIBILITY) ||
                attacker->is_affect_flag(AFF_REVIVE_INVISIBLE))
                continue;

            float dist = distance_approx(attacker->x() - chr->x(), attacker->y() - chr->y());

            if (dist < min_dist) {
                victim = attacker;
                min_dist = dist;
            }
        }

        return victim;
    }

    void character::set_victim(character *victim) {
        if (!victim) {
            if (0 != (uint32_t)vid_victim)
                monster_log("공격 대상을 해제");

            vid_victim.reset();
            battle_end(this);
        } else {
            if (vid_victim != victim->get_vid())
                monster_log("공격 대상을 설정: %s", victim->name());

            vid_victim = victim->get_vid();
            last_victim_set_time_ = get_dword_time();
        }
    }

    auto character::victim() const -> character * {
        return get_character_manager().find(vid_victim);
    }

    auto character::protege() const -> character * {
        if (chr_stone_)
            return chr_stone_;

        if (party_)
            return party_->leader();

        return nullptr;
    }

    auto character::alignment() const -> int {
        return alignment_;
    }

    auto character::real_alignment() const -> int {
        return real_alignment_;
    }

    void character::show_alignment(bool show) {
        if (show) {
            if (alignment_ != real_alignment_) {
                alignment_ = real_alignment_;
                update_packet();
            }
        } else {
            if (alignment_ != 0) {
                alignment_ = 0;
                update_packet();
            }
        }
    }

    void character::update_alignment(int amount) {
        bool show = false;

        if (alignment_ == real_alignment_)
            show = true;

        int i = alignment_ / 10;

        real_alignment_ = std::clamp(real_alignment_ + amount, -200000, 200000);

        if (show) {
            alignment_ = real_alignment_;

            if (i != alignment_ / 10)
                update_packet();
        }
    }

    void character::set_killer_mode(bool is_on) {
        if ((is_on ? ADD_CHARACTER_STATE_KILLER : 0) == is_set(add_chr_state_, ADD_CHARACTER_STATE_KILLER))
            return;

        if (is_on)
            set_bit(add_chr_state_, ADD_CHARACTER_STATE_KILLER);
        else
            remove_bit(add_chr_state_, ADD_CHARACTER_STATE_KILLER);

        killer_mode_pulse_ = thecore_pulse();
        update_packet();
        metin2::log::debug("{} (pid {:d}): killer mode updated", name(), player_id());
    }

    auto character::is_killer_mode() const -> bool {
        return is_set(add_chr_state_, ADD_CHARACTER_STATE_KILLER);
    }

    void character::update_killer_mode() {
        if (!is_killer_mode())
            return;

        int killer_seconds = 30;

        if (thecore_pulse() - killer_mode_pulse_ >= passes_per_sec(killer_seconds))
            set_killer_mode(false);
    }

    void character::set_mode(uint8_t pk_mode) {
        if (pk_mode >= PK_MODE_MAX_NUM)
            return;

        if (pk_mode_ == pk_mode)
            return;

        if (pk_mode == PK_MODE_GUILD && !get_guild())
            pk_mode = PK_MODE_FREE;

        pk_mode_ = pk_mode;
        update_packet();

        metin2::log::info("{}: PK mode set to {:d}", name(), pk_mode_);
    }

    auto character::pk_mode() const -> uint8_t {
        return pk_mode_;
    }

    void character::forget_my_attacker() {
        sectree *sec = get_sectree();
        if (sec) {
            sec->for_each_around([&](entity *ent) {
                if (ent->is_type(ENTITY_CHARACTER)) {
                    auto *ch_value = (character *)ent;
                    if (ch_value->is_pc())
                        return;
                    if (ch_value->vid_victim == get_vid())
                        ch_value->set_victim(nullptr);
                }
            });
        }
        revive_invisible(5);
    }

    void character::aggregate_monster() {
        sectree *sec = get_sectree();
        if (sec) {
            sec->for_each_around([&](entity *ent) {
                if (ent->is_type(ENTITY_CHARACTER)) {
                    auto *ch_value = (character *)ent;
                    if (ch_value->is_pc())
                        return;
                    if (!ch_value->is_monster())
                        return;
                    if (ch_value->victim())
                        return;

                    if (prob(50))
                        if (distance_approx(ch_value->x() - x(), ch_value->y() - y()) < 5000)
                            if (ch_value->can_begin_fight())
                                ch_value->begin_fight(this);
                }
            });
        }
    }

    void character::attract_ranger() {
        sectree *sec = get_sectree();
        if (sec) {
            sec->for_each_around([&](entity *ent) {
                if (ent->is_type(ENTITY_CHARACTER)) {
                    auto *ch_value = (character *)ent;
                    if (ch_value->is_pc())
                        return;
                    if (!ch_value->is_monster())
                        return;
                    if (ch_value->victim() && ch_value->victim() != this)
                        return;
                    if (ch_value->mob_attack_range() > 150) {
                        int new_range = 150;
                        if (new_range < 150)
                            new_range = 150;

                        ch_value->add_affect(AFFECT_BOW_DISTANCE, POINT_BOW_DISTANCE, new_range - ch_value->mob_attack_range(), AFF_NONE, 3 * 60, 0, false);
                    }
                }
            });
        }
    }

    void character::pull_monster() {
        sectree *sec = get_sectree();
        if (sec) {
            const int length = 300;
            sec->for_each_around([&](entity *ent) {
                if (ent->is_type(ENTITY_CHARACTER)) {
                    auto *ch_value = (character *)ent;
                    if (ch_value->is_pc())
                        return;
                    if (!ch_value->is_monster())
                        return;
                    float dist = distance_approx(x() - ch_value->x(), y() - ch_value->y());
                    if (dist > 3000 || dist < 100)
                        return;

                    float new_dist = dist - length;
                    if (new_dist < 100)
                        new_dist = 100;

                    float degree = get_degree_from_position_xy(ch_value->x(), ch_value->y(), x(), y());
                    float fx;
                    float fy;

                    get_delta_by_degree(degree, dist - new_dist, &fx, &fy);
                    auto tx = (int32_t)(ch_value->x() + fx);
                    auto ty = (int32_t)(ch_value->y() + fy);

                    ch_value->sync(tx, ty);
                    ch_value->go_to(tx, ty);
                    ch_value->calculate_move_duration();

                    ch_value->sync_packet();
                }
            });
        }
    }

    void character::update_aggr_point_ex(character *attacker, damage_type type, int dam, character::battle_info &info) {
        switch (type) {
        case DAMAGE_TYPE_NORMAL_RANGE:
            dam = (int)(dam * 1.2f);
            break;

        case DAMAGE_TYPE_RANGE:
            dam = (int)(dam * 1.5f);
            break;

        case DAMAGE_TYPE_MAGIC:
            dam = (int)(dam * 1.2f);
            break;

        default:
            break;
        }

        if (attacker == victim())
            dam = (int)(dam * 1.2f);

        info.aggro += dam;

        if (info.aggro < 0)
            info.aggro = 0;

        if (get_party() && dam > 0 && type != DAMAGE_TYPE_SPECIAL) {
            party *party = get_party();

            int party_aggro_dist = dam;

            if (party->leader_pid() == get_vid())
                party_aggro_dist /= 2;
            else
                party_aggro_dist /= 3;

            party->send_message(this, PM_AGGRO_INCREASE, party_aggro_dist, attacker->get_vid());
        }

        change_victim_by_aggro(info.aggro, attacker);
    }

    void character::update_aggr_point(character *attacker, damage_type type, int dam) {
        if (is_dead() || is_stun())
            return;

        auto it = map_damage_.find(attacker->get_vid());

        if (it == map_damage_.end()) {
            map_damage_.insert(damage_map::value_type(attacker->get_vid(), battle_info(0, dam)));
            it = map_damage_.find(attacker->get_vid());
        }

        update_aggr_point_ex(attacker, type, dam, it->second);
    }

    void character::change_victim_by_aggro(int new_aggro, character *new_victim) {
        if (get_dword_time() - last_victim_set_time_ < 3000)
            return;

        if (new_victim == victim()) {
            if (max_aggro_ < new_aggro) {
                max_aggro_ = new_aggro;
                return;
            }

            damage_map::iterator it;
            auto find = map_damage_.end();

            for (it = map_damage_.begin(); it != map_damage_.end(); ++it) {
                if (it->second.aggro > new_aggro) {
                    character *ch = get_character_manager().find(it->first);

                    if (ch && !ch->is_dead() && distance_approx(ch->x() - x(), ch->y() - y()) < 5000) {
                        find = it;
                        new_aggro = it->second.aggro;
                    }
                }
            }

            if (find != map_damage_.end()) {
                max_aggro_ = new_aggro;
                set_victim(get_character_manager().find(find->first));
                state_duration_ = 1;
            }
        } else {
            if (max_aggro_ < new_aggro) {
                max_aggro_ = new_aggro;
                set_victim(new_victim);
                state_duration_ = 1;
            }
        }
    }
}
