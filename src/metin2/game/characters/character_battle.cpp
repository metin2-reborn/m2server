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
#include <metin2/proto/length.h>

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
        packet_gc_create_fly packFly;

        packFly.header = HEADER_GC_CREATE_FLY;
        packFly.type = type_value;
        packFly.start_vid = get_vid();
        packFly.end_vid = victim->get_vid();

        packet_around(&packFly, sizeof(packet_gc_create_fly));
    }

    void character::distribute_sp(character *killer, int method) {
        if (killer->sp() >= killer->max_sp())
            return;

        bool bAttacking = (get_dword_time() - last_attack_time()) < 3000;
        bool bMoving = (get_dword_time() - last_move_time()) < 3000;

        if (method == 1) {
            int num = number(0, 3);

            if (!num) {
                int iLvDelta = level() - killer->level();
                int iAmount = 0;

                if (iLvDelta >= 5)
                    iAmount = 10;
                else if (iLvDelta >= 0)
                    iAmount = 6;
                else if (iLvDelta >= -3)
                    iAmount = 2;

                if (iAmount != 0) {
                    iAmount += (iAmount * killer->point(POINT_SP_REGEN)) / 100;

                    if (iAmount >= 11)
                        create_fly(FLY_SP_BIG, killer);
                    else if (iAmount >= 7)
                        create_fly(FLY_SP_MEDIUM, killer);
                    else
                        create_fly(FLY_SP_SMALL, killer);

                    killer->point_change(POINT_SP, iAmount);
                }
            }
        } else {
            if (killer->job() == JOB_SHAMAN || (killer->job() == JOB_SURA && killer->skill_group() == 2)) {
                int iAmount;

                if (bAttacking)
                    iAmount = 2 + max_sp() / 100;
                else if (bMoving)
                    iAmount = 3 + max_sp() * 2 / 100;
                else
                    iAmount = 10 + max_sp() * 3 / 100;

                iAmount += (iAmount * killer->point(POINT_SP_REGEN)) / 100;
                killer->point_change(POINT_SP, iAmount);
            } else {
                int iAmount;

                if (bAttacking)
                    iAmount = 2 + killer->max_sp() / 200;
                else if (bMoving)
                    iAmount = 2 + killer->max_sp() / 100;
                else {
                    if (killer->hp() < killer->max_hp())
                        iAmount = 2 + (killer->max_sp() / 100);
                    else
                        iAmount = 9 + (killer->max_sp() / 100);
                }

                iAmount += (iAmount * killer->point(POINT_SP_REGEN)) / 100;
                killer->point_change(POINT_SP, iAmount);
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

        uint32_t dwCurrentTime = get_dword_time();

        if (is_pc()) {
            if (is_speed_hack(this, victim, dwCurrentTime))
                return false;

            if (type == 0 && dwCurrentTime < get_skip_combo_attack_by_time())
                return false;
        } else {
            monster_chat(MONSTER_CHAT_ATTACK);
        }

        victim->set_sync_owner(this);

        if (victim->can_begin_fight())
            victim->begin_fight(this);

        int iRet;

        if (type == 0) {
            switch (mob_battle_type()) {
            case BATTLE_TYPE_MELEE:
            case BATTLE_TYPE_POWER:
            case BATTLE_TYPE_TANKER:
            case BATTLE_TYPE_SUPER_POWER:
            case BATTLE_TYPE_SUPER_TANKER:
                iRet = battle_melee_attack(this, victim);
                break;

            case BATTLE_TYPE_RANGE:
                fly_target(victim->get_vid(), victim->x(), victim->y(), HEADER_CG_FLY_TARGETING);
                iRet = shoot(0) ? BATTLE_DAMAGE : BATTLE_NONE;
                break;

            case BATTLE_TYPE_MAGIC:
                fly_target(victim->get_vid(), victim->x(), victim->y(), HEADER_CG_FLY_TARGETING);
                iRet = shoot(1) ? BATTLE_DAMAGE : BATTLE_NONE;
                break;

            default:
                metin2::log::error("Unhandled mob battle type {:d}", mob_battle_type());
                iRet = BATTLE_NONE;
                break;
            }
        } else {
            if (is_pc() == true) {
                if (dwCurrentTime - last_skill_time_ > 1500) {
                    metin2::log::warn("Possible skill hack by {} (pid {:d}): skill attack {:d} ms after skill use",
                        name(), player_id(), (dwCurrentTime - last_skill_time_));
                    return false;
                }
            }

            metin2::log::debug("Attack computes skill {:d} on {}", type, victim ? victim->name() : "");
            iRet = compute_skill(type, victim);
        }

        if (iRet == BATTLE_DAMAGE || iRet == BATTLE_DEAD) {
            on_move(true);
            victim->on_move();

            if (BATTLE_DEAD == iRet && is_pc())
                set_victim(nullptr);

            return true;
        }

        return false;
    }

    void character::death_penalty(uint8_t town) {
        metin2::log::debug("{}: death penalty check (town {:d})", name(), town);

        cube_close(this);

        if (battle_arena::instance().is_battle_arena_map(map_index()) == true) {
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

            int iLoss = ((next_exp() * exp_loss_percents[std::clamp(level(), 1, PLAYER_EXP_TABLE_MAX)]) / 100);

            iLoss = std::min(800000, iLoss);

            if (town) {
                iLoss = 0;
            }

            if (is_equip_unique_item(UNIQUE_ITEM_TEARDROP_OF_GODNESS))
                iLoss /= 2;

            metin2::log::info("{}: death penalty, lost {:d} exp ({:d}%)", name(), iLoss, exp_loss_percents[std::min(metin2::config::game().max_level, level())]);

            point_change(POINT_EXP, -iLoss, true);
        }
    }

    auto character::is_stun() const -> bool {
        if (is_set(points_instant_.instant_flag, INSTANT_FLAG_STUN))
            return true;

        return false;
    }

    EVENTFUNC(stun_event) {
        char_event_info *info = dynamic_cast<char_event_info *>(event->info.get());

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

        char_event_info *info = alloc_event_info<char_event_info>();

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
            ch = character_manager::instance().find_by_pid(info->id);
        } else {
            ch = character_manager::instance().find(info->id);
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

            if (sectree_manager::instance().recall_position_by_empire(ch->map_index(), ch->empire(), pos))
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

            character_manager::instance().destroy_character(ch);
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
        bool isAutoLoot =
            (attacker->premium_remain_seconds(PREMIUM_AUTOLOOT) > 0 ||
                attacker->is_equip_unique_group(UNIQUE_GROUP_AUTOLOOT))
                ? true
                : false;

        pixel_position pos;

        if (!isAutoLoot)
            if (!sectree_manager::instance().movable_position(map_index(), x(), y(), pos))
                return;

        int iTotalGold = 0;
        int iGoldPercent = mob_rank_stats[get_mob_rank()].gold_percent;

        if (attacker->is_pc())
            iGoldPercent = iGoldPercent * (100 + priv_manager::instance().priv(attacker, PRIV_GOLD_DROP)) / 100;

        if (attacker->point(POINT_MALL_GOLDBONUS))
            iGoldPercent += (iGoldPercent * attacker->point(POINT_MALL_GOLDBONUS) / 100);

        iGoldPercent = iGoldPercent * character_manager::instance().mob_gold_drop_rate(attacker) / 100;

        if (attacker->premium_remain_seconds(PREMIUM_GOLD) > 0 ||
            attacker->is_equip_unique_group(UNIQUE_GROUP_LUCKY_GOLD))
            iGoldPercent += iGoldPercent;

        if (iGoldPercent > 100)
            iGoldPercent = 100;

        int iPercent;

        if (get_mob_rank() >= MOB_RANK_BOSS)
            iPercent = ((iGoldPercent * percent_lvdelta_boss(attacker->level(), level())) / 100);
        else
            iPercent = ((iGoldPercent * percent_lvdelta(attacker->level(), level())) / 100);

        if (number(1, 100) > iPercent)
            return;

        int iGoldMultipler = get_gold_multipler();

        if (1 == number(1, 50000))
            iGoldMultipler *= 10;
        else if (1 == number(1, 10000))
            iGoldMultipler *= 5;

        if (attacker->point(POINT_GOLD_DOUBLE_BONUS))
            if (prob(attacker->point(POINT_GOLD_DOUBLE_BONUS)))
                iGoldMultipler *= 2;

        if (metin2::config::test_server())
            attacker->chat_packet(CHAT_TYPE_PARTY, "gold_mul %d rate %d", iGoldMultipler, character_manager::instance().mob_gold_amount_rate(attacker));

        item *item;

        int iGold10DropPct = 100;
        iGold10DropPct = (iGold10DropPct * 100) / (100 + priv_manager::instance().priv(attacker, PRIV_GOLD10_DROP));

        if (get_mob_rank() >= MOB_RANK_BOSS && !is_stone() && get_mob_table().gold_max != 0) {
            if (1 == number(1, iGold10DropPct))
                iGoldMultipler *= 10;

            int iSplitCount = number(25, 35);

            for (int i = 0; i < iSplitCount; ++i) {
                int iGold = number(get_mob_table().gold_min, get_mob_table().gold_max) / iSplitCount;
                if (metin2::config::test_server())
                    metin2::log::debug("Gold drop: base amount {:d}", iGold);
                iGold = iGold * character_manager::instance().mob_gold_amount_rate(attacker) / 100;
                iGold *= iGoldMultipler;

                if (iGold == 0) {
                    continue;
                }

                if (metin2::config::test_server()) {
                    metin2::log::debug("Gold drop: rate {:d}%, multiplier {:d}", character_manager::instance().mob_gold_amount_rate(attacker), iGoldMultipler);
                    metin2::log::debug("Gold drop: {:d} (min {:d}, max {:d})", iGold, get_mob_table().gold_min, get_mob_table().gold_max);
                }

                if ((item = item_manager::instance().create_item(1, iGold))) {
                    pos.x = x() + ((number(-14, 14) + number(-14, 14)) * 23);
                    pos.y = y() + ((number(-14, 14) + number(-14, 14)) * 23);

                    item->add_to_ground(map_index(), pos);
                    item->start_destroy_event();

                    iTotalGold += iGold;
                }
            }
        } else if (1 == number(1, iGold10DropPct)) {
            for (int i = 0; i < 10; ++i) {
                int iGold = number(get_mob_table().gold_min, get_mob_table().gold_max);
                iGold = iGold * character_manager::instance().mob_gold_amount_rate(attacker) / 100;
                iGold *= iGoldMultipler;

                if (iGold == 0) {
                    continue;
                }

                if ((item = item_manager::instance().create_item(1, iGold))) {
                    pos.x = x() + (number(-7, 7) * 20);
                    pos.y = y() + (number(-7, 7) * 20);

                    item->add_to_ground(map_index(), pos);
                    item->start_destroy_event();

                    iTotalGold += iGold;
                }
            }
        } else {
            int iGold = number(get_mob_table().gold_min, get_mob_table().gold_max);
            iGold = iGold * character_manager::instance().mob_gold_amount_rate(attacker) / 100;
            iGold *= iGoldMultipler;

            int iSplitCount;

            if (iGold >= 3)
                iSplitCount = number(1, 3);
            else if (get_mob_rank() >= MOB_RANK_BOSS) {
                iSplitCount = number(3, 10);

                if ((iGold / iSplitCount) == 0)
                    iSplitCount = 1;
            } else
                iSplitCount = 1;

            if (iGold != 0) {
                iTotalGold += iGold;

                for (int i = 0; i < iSplitCount; ++i) {
                    if (isAutoLoot) {
                        attacker->give_gold(iGold / iSplitCount);
                    } else if ((item = item_manager::instance().create_item(1, iGold / iSplitCount))) {
                        pos.x = x() + (number(-7, 7) * 20);
                        pos.y = y() + (number(-7, 7) * 20);

                        item->add_to_ground(map_index(), pos);
                        item->start_destroy_event();
                    }
                }
            }
        }

        db_manager::instance().send_money_log(MONEY_LOG_MONSTER, race_num(), iTotalGold);
    }

    void character::reward(bool item_drop) {
        if (race_num() == 5001) {
            pixel_position pos;

            if (!sectree_manager::instance().movable_position(map_index(), x(), y(), pos))
                return;

            item *item;
            int iGold = number(get_mob_table().gold_min, get_mob_table().gold_max);
            iGold = iGold * character_manager::instance().mob_gold_amount_rate(nullptr) / 100;
            iGold *= get_gold_multipler();
            int iSplitCount = number(25, 35);

            metin2::log::debug("Waegu died: gold {:d} split into {:d}", iGold, iSplitCount);

            for (int i = 1; i <= iSplitCount; ++i) {
                if ((item = item_manager::instance().create_item(1, iGold / iSplitCount))) {
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

        character *pkAttacker = distribute_exp();

        if (!pkAttacker)
            return;

        if (pkAttacker->is_pc()) {
            if (level() - pkAttacker->level() >= -10) {
                if (pkAttacker->real_alignment() < 0) {
                    if (pkAttacker->is_equip_unique_item(UNIQUE_ITEM_FASTER_ALIGNMENT_UP_BY_KILL))
                        pkAttacker->update_alignment(14);
                    else
                        pkAttacker->update_alignment(7);
                } else
                    pkAttacker->update_alignment(2);
            }

            pkAttacker->set_quest_npcid(get_vid());
            quest::quest_manager::instance().kill(pkAttacker->player_id(), race_num());
            character_manager::instance().kill_log(race_num());

            if (!number(0, 9)) {
                if (pkAttacker->point(POINT_KILL_HP_RECOVERY)) {
                    int iHP = pkAttacker->max_hp() * pkAttacker->point(POINT_KILL_HP_RECOVERY) / 100;
                    pkAttacker->point_change(POINT_HP, iHP);
                    create_fly(FLY_HP_SMALL, pkAttacker);
                }

                if (pkAttacker->point(POINT_KILL_SP_RECOVER)) {
                    int iSP = pkAttacker->max_sp() * pkAttacker->point(POINT_KILL_SP_RECOVER) / 100;
                    pkAttacker->point_change(POINT_SP, iSP);
                    create_fly(FLY_SP_SMALL, pkAttacker);
                }
            }
        }

        if (!item_drop)
            return;

        pixel_position pos = xyz();

        if (!sectree_manager::instance().movable_position(map_index(), pos.x, pos.y, pos))
            return;

        if (metin2::config::test_server())
            metin2::log::debug("Gold reward for {}", pkAttacker->name());
        reward_gold(pkAttacker);

        game::items::item *item;

        static std::vector<game::items::item *> s_vec_item;
        s_vec_item.clear();

        if (item_manager::instance().create_drop_item(this, pkAttacker, s_vec_item)) {
            if (s_vec_item.size() == 0)
                ;
            else if (s_vec_item.size() == 1) {
                item = s_vec_item[0];
                item->add_to_ground(map_index(), pos);

                if (battle_arena::instance().is_battle_arena_map(pkAttacker->map_index()) == false) {
                    item->set_ownership(pkAttacker);
                }

                item->start_destroy_event();

                pos.x = number(-7, 7) * 20;
                pos.y = number(-7, 7) * 20;
                pos.x += x();
                pos.y += y();

                metin2::log::debug("Item {} dropped at {:d} {:d} from {}", item->name(), pos.x, pos.y, name());
            } else {
                int iItemIdx = s_vec_item.size() - 1;

                std::priority_queue<std::pair<int, character *>> pq;

                int total_dam = 0;

                for (damage_map::iterator it = map_damage_.begin(); it != map_damage_.end(); ++it) {
                    int iDamage = it->second.total_damage;
                    if (iDamage > 0) {
                        character *ch = character_manager::instance().find(it->first);

                        if (ch) {
                            pq.push(std::make_pair(iDamage, ch));
                            total_dam += iDamage;
                        }
                    }
                }

                std::vector<character *> v;

                while (!pq.empty() && pq.top().first * 10 >= total_dam) {
                    v.push_back(pq.top().second);
                    pq.pop();
                }

                if (v.empty()) {
                    while (iItemIdx >= 0) {
                        item = s_vec_item[iItemIdx--];

                        if (!item) {
                            metin2::log::error("Null item at drop list index {:d}", iItemIdx + 1);
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
                    std::vector<character *>::iterator it = v.begin();

                    while (iItemIdx >= 0) {
                        item = s_vec_item[iItemIdx--];

                        if (!item) {
                            metin2::log::error("Null item at drop list index {:d}", iItemIdx + 1);
                            continue;
                        }

                        item->add_to_ground(map_index(), pos);

                        character *ch = *it;

                        if (ch->get_party())
                            ch = ch->get_party()->next_ownership(ch, x(), y());

                        ++it;

                        if (it == v.end())
                            it = v.begin();

                        if (battle_arena::instance().is_battle_arena_map(ch->map_index()) == false) {
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

        if (battle_arena::instance().is_battle_arena_map(map_index()) == true) {
            return;
        }

        struct game::characters::item_drop_penalty *table = &item_drop_penalty_kor[0];

        if (level() < 10)
            return;

        int iAlignIndex;

        if (real_alignment() >= 120000)
            iAlignIndex = 0;
        else if (real_alignment() >= 80000)
            iAlignIndex = 1;
        else if (real_alignment() >= 40000)
            iAlignIndex = 2;
        else if (real_alignment() >= 10000)
            iAlignIndex = 3;
        else if (real_alignment() >= 0)
            iAlignIndex = 4;
        else if (real_alignment() > -40000)
            iAlignIndex = 5;
        else if (real_alignment() > -80000)
            iAlignIndex = 6;
        else if (real_alignment() > -120000)
            iAlignIndex = 7;
        else
            iAlignIndex = 8;

        std::vector<std::pair<item *, int>> vec_item;
        item *pkItem;
        int i;
        bool isDropAllEquipments = false;

        game::characters::item_drop_penalty &r = table[iAlignIndex];
        metin2::log::debug("{}: PK drop penalty for alignment index {:d}, inventory {:d}%, equipment {:d}%", name(), iAlignIndex, r.inventory_pct, r.equipment_pct);

        bool bDropInventory = r.inventory_pct >= number(1, 1000);
        bool bDropEquipment = r.equipment_pct >= number(1, 100);
        bool bDropAntiDropUniqueItem = false;

        if ((bDropInventory || bDropEquipment) && is_equip_unique_item(UNIQUE_ITEM_SKIP_ITEM_DROP_PENALTY)) {
            bDropInventory = false;
            bDropEquipment = false;
            bDropAntiDropUniqueItem = true;
        }

        if (bDropInventory) {
            std::vector<uint8_t> vec_bSlots;

            for (i = 0; i < INVENTORY_MAX_NUM; ++i)
                if (inventory_item(i))
                    vec_bSlots.push_back(i);

            if (!vec_bSlots.empty()) {
                std::ranges::shuffle(vec_bSlots, random_engine());

                int iQty = std::min<int>(vec_bSlots.size(), r.inventory_qty);

                if (iQty)
                    iQty = number(1, iQty);

                for (i = 0; i < iQty; ++i) {
                    pkItem = inventory_item(vec_bSlots[i]);

                    if (is_set(pkItem->anti_flag(), ITEM_ANTIFLAG_GIVE | ITEM_ANTIFLAG_PKDROP))
                        continue;

                    sync_quickslot(QUICKSLOT_TYPE_ITEM, vec_bSlots[i], 255);
                    vec_item.push_back(std::make_pair(pkItem->remove_from_character(), INVENTORY));
                }
            } else if (iAlignIndex == 8)
                isDropAllEquipments = true;
        }

        if (bDropEquipment) {
            std::vector<uint8_t> vec_bSlots;

            for (i = 0; i < WEAR_MAX_NUM; ++i)
                if (wear(i))
                    vec_bSlots.push_back(i);

            if (!vec_bSlots.empty()) {
                std::ranges::shuffle(vec_bSlots, random_engine());
                int iQty;

                if (isDropAllEquipments)
                    iQty = vec_bSlots.size();
                else
                    iQty = std::min<int>(vec_bSlots.size(), number(1, r.equipment_qty));

                if (iQty)
                    iQty = number(1, iQty);

                for (i = 0; i < iQty; ++i) {
                    pkItem = wear(vec_bSlots[i]);

                    if (is_set(pkItem->anti_flag(), ITEM_ANTIFLAG_GIVE | ITEM_ANTIFLAG_PKDROP))
                        continue;

                    sync_quickslot(QUICKSLOT_TYPE_ITEM, vec_bSlots[i], 255);
                    vec_item.push_back(std::make_pair(pkItem->remove_from_character(), EQUIPMENT));
                }
            }
        }

        if (bDropAntiDropUniqueItem) {
            item *pkItem;

            pkItem = wear(WEAR_UNIQUE1);

            if (pkItem && pkItem->vnum() == UNIQUE_ITEM_SKIP_ITEM_DROP_PENALTY) {
                sync_quickslot(QUICKSLOT_TYPE_ITEM, WEAR_UNIQUE1, 255);
                vec_item.push_back(std::make_pair(pkItem->remove_from_character(), EQUIPMENT));
            }

            pkItem = wear(WEAR_UNIQUE2);

            if (pkItem && pkItem->vnum() == UNIQUE_ITEM_SKIP_ITEM_DROP_PENALTY) {
                sync_quickslot(QUICKSLOT_TYPE_ITEM, WEAR_UNIQUE2, 255);
                vec_item.push_back(std::make_pair(pkItem->remove_from_character(), EQUIPMENT));
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
                log_manager::instance().item_log(this, item, "DEAD_DROP", (window == INVENTORY) ? "INVENTORY" : ((window == EQUIPMENT) ? "EQUIPMENT" : ""));

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
            killer = character_manager::instance().find_by_pid(killer_pid_);

        killer_pid_ = 0; // Must be reset here, or a later death is credited to this killer.

        bool isAgreedPVP = false;
        bool isUnderGuildWar = false;
        bool isDuel = false;
        bool isForked = false;

        if (killer && killer->is_pc()) {
            if (killer->chr_target_ == this)
                killer->set_target(nullptr);

            if (!is_pc() && killer->get_dungeon())
                killer->get_dungeon()->inc_kill_count(killer, this);

            isAgreedPVP = pvp_manager::instance().dead(this, killer->player_id());
            isDuel = arena_manager::instance().on_dead(killer, this);

            if (is_pc()) {
                guild *g1 = get_guild();
                guild *g2 = killer->get_guild();

                if (g1 && g2)
                    if (g1->under_war(g2->id()))
                        isUnderGuildWar = true;

                killer->set_quest_npcid(get_vid());
                quest::quest_manager::instance().kill(killer->player_id(), quest::QUEST_NO_NPC);
                guild_manager::instance().kill(killer, this);
            }
        }

        if (is_pc()) {
            if (three_way_war::instance().is_three_way_war_map_index(map_index()))
                isForked = true;
        }

        if (killer &&
            !isAgreedPVP &&
            !isUnderGuildWar &&
            is_pc() &&
            !isDuel &&
            !isForked &&
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

        if (true == isForked) {
            three_way_war::instance().on_dead(this, killer);
        }

        set_position(POS_DEAD);
        clear_affect(true);

        if (killer && is_pc()) {
            if (!killer->is_pc()) {
                if (!isForked) {
                    metin2::log::debug("{} ({}) died with penalty", name(), static_cast<const void *>(this));
                    set_bit(points_instant_.instant_flag, INSTANT_FLAG_DEATH_PENALTY);
                    log_manager::instance().char_log(this, killer->race_num(), "DEAD_BY_NPC", killer->name());
                }
            } else {
                metin2::log::debug("{} ({}) killed by player {} ({})", name(), static_cast<const void *>(this), killer->name(), static_cast<const void *>(killer));
                remove_bit(points_instant_.instant_flag, INSTANT_FLAG_DEATH_PENALTY);

                if (empire() != killer->empire()) {
                    int iEP = std::min(point(POINT_EMPIRE_POINT), killer->point(POINT_EMPIRE_POINT));

                    point_change(POINT_EMPIRE_POINT, -(iEP / 10));
                    killer->point_change(POINT_EMPIRE_POINT, iEP / 5);

                    if (point(POINT_EMPIRE_POINT) < 10) {
                    }

                    const std::string buf = std::format("{} {} {} {} {} {} {} {}",
                        empire(), alignment(), pk_mode(), name(),
                        killer->empire(), killer->alignment(), killer->pk_mode(), killer->name());

                    log_manager::instance().char_log(this, killer->player_id(), "DEAD_BY_PC", buf.c_str());
                } else {
                    if (!isAgreedPVP && !isUnderGuildWar && !is_killer_mode() && alignment() >= 0 && !isDuel && !isForked) {
                        int iNoPenaltyProb = 0;

                        if (killer->alignment() >= 0)
                            iNoPenaltyProb = 33;
                        else
                            iNoPenaltyProb = 20;

                        if (number(1, 100) < iNoPenaltyProb)
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

                    log_manager::instance().char_log(this, killer->player_id(), "DEAD_BY_PC", buf.c_str());
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
                        character *chResurrect = character_manager::instance().spawn_mob(get_mob_table().resurrection_vnum, map_index(), x(), y(), z(), true, (int)rotation());
                        if (get_dungeon() && chResurrect) {
                            chResurrect->set_dungeon(get_dungeon());
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
                log_manager::instance().char_log(killer, race_num(), "STONE_KILL", buf.c_str());
            else
                log_manager::instance().char_log(killer, race_num(), "BOSS_KILL", buf.c_str());
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

        if (isDuel == false) {
            if (dead_event) {
                metin2::log::debug("{} ({}): dead event cancelled ({})", name(), static_cast<const void *>(this), static_cast<const void *>(dead_event.get()));
                event_cancel(&dead_event);
            }

            if (is_stone())
                clear_stone();

            if (get_dungeon()) {
                get_dungeon()->dead_character(this);
            }

            char_dead_event_info *pEventInfo = alloc_event_info<char_dead_event_info>();

            if (is_pc()) {
                pEventInfo->is_pc = true;
                pEventInfo->id = this->player_id();

                dead_event = event_create(game::characters::dead_event, pEventInfo, passes_per_sec(180));
            } else {
                pEventInfo->is_pc = false;
                pEventInfo->id = this->get_vid();

                if (is_revive() == false && has_reviver_in_party() == true) {
                    dead_event = event_create(game::characters::dead_event, pEventInfo, immediate_dead ? 1 : passes_per_sec(3));
                } else {
                    dead_event = event_create(game::characters::dead_event, pEventInfo, immediate_dead ? 1 : passes_per_sec(10));
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

        shop_manager::instance().stop_shopping(this);
        close_my_shop();
        close_safebox();

        if (true == is_monster() && 2493 == get_mob_table().vnum) {
            if (nullptr != killer && nullptr != killer->get_guild()) {
                dragon_lair_manager::instance().on_dragon_dead(this, killer->get_guild()->id());
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
            packet_gc_damage_info damageInfo;
            memset(&damageInfo, 0, sizeof(packet_gc_damage_info));

            damageInfo.header = HEADER_GC_DAMAGE_INFO;
            damageInfo.vid = (uint32_t)get_vid();
            damageInfo.flag = damage_flag;
            damageInfo.damage = damage_value;

            if (get_desc() != nullptr) {
                get_desc()->packet(&damageInfo, sizeof(packet_gc_damage_info));
            }

            if (attacker->get_desc() != nullptr) {
                attacker->get_desc()->packet(&damageInfo, sizeof(packet_gc_damage_info));
            }
        }
    }

    auto character::damage(character *attacker, int dam, damage_type type) -> bool {
        if (DAMAGE_TYPE_MAGIC == type) {
            dam = (int)((float)dam * (100 + (attacker->point(POINT_MAGIC_ATT_BONUS_PER) + attacker->point(POINT_MELEE_MAGIC_ATT_BONUS_PER))) / 100.f + 0.5f);
        }
        if (race_num() == 5001) {
            bool bDropMoney = false;
            int iPercent = (hp() * 100) / max_hp();

            if (iPercent <= 10 && max_sp() < 5) {
                set_max_sp(5);
                bDropMoney = true;
            } else if (iPercent <= 20 && max_sp() < 4) {
                set_max_sp(4);
                bDropMoney = true;
            } else if (iPercent <= 40 && max_sp() < 3) {
                set_max_sp(3);
                bDropMoney = true;
            } else if (iPercent <= 60 && max_sp() < 2) {
                set_max_sp(2);
                bDropMoney = true;
            } else if (iPercent <= 80 && max_sp() < 1) {
                set_max_sp(1);
                bDropMoney = true;
            }

            if (bDropMoney) {
                uint32_t dwGold = 1000;
                int iSplitCount = number(10, 13);

                metin2::log::debug("Waegu drops gold on hit {:d} times", max_sp());

                for (int i = 1; i <= iSplitCount; ++i) {
                    pixel_position pos;
                    item *item;

                    if ((item = item_manager::instance().create_item(1, dwGold / iSplitCount))) {
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

        int iCurHP = hp();
        int iCurSP = sp();

        bool IsCritical = false;
        bool IsPenetrate = false;
        bool IsDeathBlow = false;

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
                int iCriticalPct = attacker->point(POINT_CRITICAL_PCT);

                if (!is_pc())
                    iCriticalPct += attacker->marriage_bonus(UNIQUE_ITEM_MARRIAGE_CRITICAL_BONUS);

                if (iCriticalPct) {
                    if (iCriticalPct >= 10)
                        iCriticalPct = 5 + (iCriticalPct - 10) / 4;
                    else
                        iCriticalPct /= 2;

                    iCriticalPct -= point(POINT_RESIST_CRITICAL);

                    if (prob(iCriticalPct)) {
                        IsCritical = true;
                        dam *= 2;
                        effect_packet(SE_CRITICAL);

                        if (is_affect_flag(AFF_MANASHIELD)) {
                            remove_affect(AFF_MANASHIELD);
                        }
                    }
                }

                int iPenetratePct = attacker->point(POINT_PENETRATE_PCT);

                if (!is_pc())
                    iPenetratePct += attacker->marriage_bonus(UNIQUE_ITEM_MARRIAGE_PENETRATE_BONUS);

                if (iPenetratePct) {
                    {
                        skill_proto *pkSk = skill_manager::instance().get(SKILL_RESIST_PENETRATE);

                        if (nullptr != pkSk) {
                            pkSk->set_point_var("k", 1.0f * skill_power(SKILL_RESIST_PENETRATE) / 100.0f);

                            iPenetratePct -= static_cast<int>(pkSk->point_poly.eval());
                        }
                    }

                    if (iPenetratePct >= 10) {
                        iPenetratePct = 5 + (iPenetratePct - 10) / 4;
                    } else {
                        iPenetratePct /= 2;
                    }

                    iPenetratePct -= point(POINT_RESIST_PENETRATE);

                    if (prob(iPenetratePct)) {
                        IsPenetrate = true;

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
                        int reflectDamage = dam * point(POINT_REFLECT_MELEE) / 100;

                        if (attacker->is_immune(IMMUNE_REFLECT))
                            reflectDamage = int(reflectDamage / 3.0f + 0.5f);

                        attacker->damage(this, reflectDamage, DAMAGE_TYPE_SPECIAL);
                    }
                }

                int iCriticalPct = attacker->point(POINT_CRITICAL_PCT);

                if (!is_pc())
                    iCriticalPct += attacker->marriage_bonus(UNIQUE_ITEM_MARRIAGE_CRITICAL_BONUS);

                if (iCriticalPct) {
                    iCriticalPct -= point(POINT_RESIST_CRITICAL);

                    if (prob(iCriticalPct)) {
                        IsCritical = true;
                        dam *= 2;
                        effect_packet(SE_CRITICAL);
                    }
                }

                int iPenetratePct = attacker->point(POINT_PENETRATE_PCT);

                if (!is_pc())
                    iPenetratePct += attacker->marriage_bonus(UNIQUE_ITEM_MARRIAGE_PENETRATE_BONUS);

                {
                    skill_proto *pkSk = skill_manager::instance().get(SKILL_RESIST_PENETRATE);

                    if (nullptr != pkSk) {
                        pkSk->set_point_var("k", 1.0f * skill_power(SKILL_RESIST_PENETRATE) / 100.0f);

                        iPenetratePct -= static_cast<int>(pkSk->point_poly.eval());
                    }
                }

                if (iPenetratePct) {

                    iPenetratePct -= point(POINT_RESIST_PENETRATE);

                    if (prob(iPenetratePct)) {
                        IsPenetrate = true;

                        if (metin2::config::test_server())
                            chat_packet(CHAT_TYPE_INFO, lc_text("관통 추가 데미지 %d"), point(POINT_DEF_GRADE) * (100 + point(POINT_DEF_BONUS)) / 100);
                        dam += point(POINT_DEF_GRADE) * (100 + point(POINT_DEF_BONUS)) / 100;
                    }
                }

                if (attacker->point(POINT_STEAL_HP)) {
                    int pct = 1;

                    if (number(1, 10) <= pct) {
                        int iHP = std::min(dam, std::max(0, iCurHP)) * attacker->point(POINT_STEAL_HP) / 100;

                        if (iHP > 0 && hp() >= iHP) {
                            create_fly(FLY_HP_SMALL, attacker);
                            attacker->point_change(POINT_HP, iHP);
                            point_change(POINT_HP, -iHP);
                        }
                    }
                }

                if (attacker->point(POINT_STEAL_SP)) {
                    int pct = 1;

                    if (number(1, 10) <= pct) {
                        int iCur;

                        if (is_pc())
                            iCur = iCurSP;
                        else
                            iCur = iCurHP;

                        int iSP = std::min(dam, std::max(0, iCur)) * attacker->point(POINT_STEAL_SP) / 100;

                        if (iSP > 0 && iCur >= iSP) {
                            create_fly(FLY_SP_SMALL, attacker);
                            attacker->point_change(POINT_SP, iSP);

                            if (is_pc())
                                point_change(POINT_SP, -iSP);
                        }
                    }
                }

                if (attacker->point(POINT_STEAL_GOLD)) {
                    if (prob(attacker->point(POINT_STEAL_GOLD))) {
                        int iAmount = number(1, level());
                        attacker->point_change(POINT_GOLD, iAmount);
                        db_manager::instance().send_money_log(MONEY_LOG_MISC, 1, iAmount);
                    }
                }

                if (attacker->point(POINT_HIT_HP_RECOVERY) && number(0, 4) > 0) {
                    int i = ((iCurHP >= 0) ? std::min(dam, iCurHP) : dam) * attacker->point(POINT_HIT_HP_RECOVERY) / 100;

                    if (i) {
                        create_fly(FLY_HP_SMALL, attacker);
                        attacker->point_change(POINT_HP, i);
                    }
                }

                if (attacker->point(POINT_HIT_SP_RECOVERY) && number(0, 4) > 0) {
                    int i = ((iCurHP >= 0) ? std::min(dam, iCurHP) : dam) * attacker->point(POINT_HIT_SP_RECOVERY) / 100;

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
            int iDamageSPPart = dam / 3;
            int iDamageToSP = iDamageSPPart * point(POINT_MANASHIELD) / 100;
            int iSP = sp();

            if (iDamageToSP <= iSP) {
                point_change(POINT_SP, -iDamageToSP);
                dam -= iDamageSPPart;
            } else {
                point_change(POINT_SP, -sp());
                dam -= iSP * 100 / std::max(point(POINT_MANASHIELD), 1);
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

            int iEmpire = empire();
            int32_t lMapIndex = map_index();
            int iMapEmpire = sectree_manager::instance().empire_from_map_index(lMapIndex);

            if (attacker->is_pc()) {
                iEmpire = attacker->empire();
                lMapIndex = attacker->map_index();
                iMapEmpire = sectree_manager::instance().empire_from_map_index(lMapIndex);

                if (iEmpire && iMapEmpire && iEmpire != iMapEmpire) {
                    int percent = 10;

                    if (184 <= lMapIndex && lMapIndex <= 189) {
                        percent = 9;
                    } else {
                        percent = 9;
                    }

                    dam = dam * percent / 10;
                }

                if (!is_pc() && monster_drain_sp_point()) {
                    int iDrain = monster_drain_sp_point();

                    if (iDrain <= attacker->sp())
                        attacker->point_change(POINT_SP, -iDrain);
                    else {
                        int iSP = attacker->sp();
                        attacker->point_change(POINT_SP, -iSP);
                    }
                }

            } else if (attacker->is_guard_npc()) {
                set_bit(points_instant_.instant_flag, INSTANT_FLAG_NO_REWARD);
                stun();
                return true;
            }

            if (attacker->is_pc() && monarch::instance().is_power_up(attacker->empire())) {
                dam += dam / 10;
            }

            if (is_pc() && monarch::instance().is_defence_up(empire())) {
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
                        IsDeathBlow = true;
                        dam = dam * 4;
                    }
                }
            }

            dam = blue_dragon_damage(this, attacker, dam);

            uint8_t damageFlag = 0;

            if (type == DAMAGE_TYPE_POISON)
                damageFlag = DAMAGE_POISON;
            else
                damageFlag = DAMAGE_NORMAL;

            if (IsCritical == true)
                damageFlag |= DAMAGE_CRITICAL;

            if (IsPenetrate == true)
                damageFlag |= DAMAGE_PENETRATE;

            float damMul = this->dam_mul();
            float tempDam = dam;
            dam = tempDam * damMul + 0.5f;

            if (attacker)
                send_damage_packet(attacker, dam, damageFlag);

            if (metin2::config::test_server()) {
                if (attacker) {
                    attacker->chat_packet(CHAT_TYPE_INFO, "-> %s, DAM %d HP %d(%d%%) %s%s",
                        name(),
                        dam,
                        hp(),
                        (hp() * 100) / max_hp(),
                        IsCritical ? "crit " : "",
                        IsPenetrate ? "pene " : "",
                        IsDeathBlow ? "deathblow " : "");
                }

                chat_packet(CHAT_TYPE_PARTY, "<- %s, DAM %d HP %d(%d%%) %s%s",
                    attacker ? attacker->name() : nullptr,
                    dam,
                    hp(),
                    (hp() * 100) / max_hp(),
                    IsCritical ? "crit " : "",
                    IsPenetrate ? "pene " : "",
                    IsDeathBlow ? "deathblow " : "");
            }

            if (detail_log_) {
                chat_packet(CHAT_TYPE_INFO, lc_text("%s[%d]가 공격 위치: %d %d"), attacker->name(), (uint32_t)attacker->get_vid(), attacker->x(), attacker->y());
            }
        }

        if (!cannot_dead_) {
            point_change(POINT_HP, -dam, false);
        }

        if (attacker && dam > 0 && is_npc()) {
            damage_map::iterator it = map_damage_.find(attacker->get_vid());

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

        int iBaseExp = exp;

        exp = exp * (100 + priv_manager::instance().priv(to, PRIV_EXP_PCT)) / 100;

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

        exp = exp * character_manager::instance().mob_exp_rate(to) / 100;

        exp = std::min<int>(to->next_exp() / 10, exp);

        if (metin2::config::test_server()) {
            if (quest::quest_manager::instance().event_flag("exp_bonus_log") && iBaseExp > 0)
                to->chat_packet(CHAT_TYPE_INFO, "exp bonus %d%%", (exp - iBaseExp) * 100 / iBaseExp);
        }

        exp = adjust_exp_by_level(to, exp);

        to->point_change(POINT_EXP, exp, true);
        from->create_fly(FLY_EXP, to);

        {
            character *you = to->marry_partner();
            if (you) {
                uint32_t dwUpdatePoint = 2000 * exp / to->level() / to->level() / 3;

                if (to->premium_remain_seconds(PREMIUM_MARRIAGE_FAST) > 0 ||
                    you->premium_remain_seconds(PREMIUM_MARRIAGE_FAST) > 0)
                    dwUpdatePoint = (uint32_t)(dwUpdatePoint * 3);

                marriage::marriage *pMarriage = marriage::manager::instance().get(to->player_id());

                if (pMarriage && pMarriage->is_near())
                    pMarriage->update(dwUpdatePoint);
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
                    if (quest::quest_manager::instance().event_flag("exp_bonus_log") && party->exp_bonus_percent())
                        party->chat_packet_to_all_member(CHAT_TYPE_INFO, "exp party bonus %d%%", party->exp_bonus_percent());
                }

                if (party->exp_centralize_character()) {
                    character *tch = party->exp_centralize_character();

                    if (distance_approx(ch->x() - tch->x(), ch->y() - tch->y()) <= PARTY_DEFAULT_RANGE) {
                        int iExpCenteralize = (int)(exp * 0.05f);
                        exp -= iExpCenteralize;

                        give_exp(ch, party->exp_centralize_character(), iExpCenteralize);
                    }
                }

                const uint32_t i_exp = exp;
                const int mode = party->exp_distribution_mode();

                if (member_count == 0)
                    member_count = 1;

                party->for_each_online_member([&](character *member) {
                    if (distance_approx(member->x() - x, member->y() - y) <= PARTY_DEFAULT_RANGE) {
                        uint32_t iExp2 = 0;

                        switch (mode) {
                        case PARTY_EXP_DISTRIBUTION_NON_PARITY:
                            iExp2 = (uint32_t)(i_exp * (float)party_exp_distribute_table[member->level()] / total);
                            break;

                        case PARTY_EXP_DISTRIBUTION_PARITY:
                            iExp2 = i_exp / member_count;
                            break;

                        default:
                            metin2::log::error("Unknown party exp distribution mode {:d}", mode);
                            return;
                        }

                        give_exp(ch, member, iExp2);
                    }
                });
            }
        }
    };

    auto character::distribute_exp() -> character * {
        int iExpToDistribute = get_exp();

        if (iExpToDistribute <= 0)
            return nullptr;

        int iTotalDam = 0;
        character *pkChrMostAttacked = nullptr;
        int iMostDam = 0;

        typedef std::vector<damage_info> damage_info_list;
        damage_info_list damage_info_table;
        std::map<party *, damage_info> map_party_damage;

        damage_info_table.reserve(map_damage_.size());

        damage_map::iterator it = map_damage_.begin();

        while (it != map_damage_.end()) {
            const vid &c_VID = it->first;
            int iDam = it->second.total_damage;

            ++it;

            character *pAttacker = character_manager::instance().find(c_VID);

            if (!pAttacker || pAttacker->is_npc() || distance_approx(x() - pAttacker->x(), y() - pAttacker->y()) > 5000)
                continue;

            iTotalDam += iDam;
            if (!pkChrMostAttacked || iDam > iMostDam) {
                pkChrMostAttacked = pAttacker;
                iMostDam = iDam;
            }

            if (pAttacker->get_party()) {
                std::map<party *, damage_info>::iterator it = map_party_damage.find(pAttacker->get_party());
                if (it == map_party_damage.end()) {
                    damage_info di;
                    di.dam = iDam;
                    di.attacker = nullptr;
                    di.party = pAttacker->get_party();
                    map_party_damage.insert(std::make_pair(di.party, di));
                } else {
                    it->second.dam += iDam;
                }
            } else {
                damage_info di;

                di.dam = iDam;
                di.attacker = pAttacker;
                di.party = nullptr;

                damage_info_table.push_back(di);
            }
        }

        for (std::map<party *, damage_info>::iterator it = map_party_damage.begin(); it != map_party_damage.end(); ++it) {
            damage_info_table.push_back(it->second);
        }

        set_exp(0);

        if (iTotalDam == 0)
            return nullptr;

        if (chr_stone_) {
            int iExp = iExpToDistribute >> 1;
            chr_stone_->set_exp(chr_stone_->get_exp() + iExp);
            iExpToDistribute -= iExp;
        }

        metin2::log::debug("{}: distributing {:d} exp among {:d} attackers (total damage {:d})",
            name(), iExpToDistribute, damage_info_table.size(), iTotalDam);

        if (damage_info_table.empty())
            return nullptr;

        distribute_hp(pkChrMostAttacked);

        {
            damage_info_list::iterator di = damage_info_table.begin();
            {
                damage_info_list::iterator it;

                for (it = damage_info_table.begin(); it != damage_info_table.end(); ++it) {
                    if (it->dam > di->dam)
                        di = it;
                }
            }

            int iExp = iExpToDistribute / 5;
            iExpToDistribute -= iExp;

            float fPercent = (float)di->dam / iTotalDam;

            if (fPercent > 1.0f) {
                metin2::log::error("Exp share {:f} over 1.0 for {}", fPercent, di->attacker->name());
                fPercent = 1.0f;
            }

            iExp += (int)(iExpToDistribute * fPercent);

            di->distribute(this, iExp);

            if (fPercent == 1.0f)
                return pkChrMostAttacked;

            di->clear();
        }

        {
            damage_info_list::iterator it;

            for (it = damage_info_table.begin(); it != damage_info_table.end(); ++it) {
                damage_info &di = *it;

                float fPercent = (float)di.dam / iTotalDam;

                if (fPercent > 1.0f) {
                    metin2::log::error("Exp share {:f} over 1.0 for {}", fPercent, di.attacker->name());
                    fPercent = 1.0f;
                }

                di.distribute(this, (int)(iExpToDistribute * fPercent));
            }
        }

        return pkChrMostAttacked;
    }

    auto character::arrow_and_bow(item **ppk_bow, item **ppk_arrow, int arrow_count) -> int {
        item *pkBow;

        if (!(pkBow = wear(WEAR_WEAPON)) || pkBow->proto()->sub_type != WEAPON_BOW) {
            return 0;
        }

        item *pkArrow;

        if (!(pkArrow = wear(WEAR_ARROW)) || pkArrow->type() != ITEM_WEAPON ||
            pkArrow->proto()->sub_type != WEAPON_ARROW) {
            return 0;
        }

        arrow_count = std::min<int>(arrow_count, pkArrow->count());

        *ppk_bow = pkBow;
        *ppk_arrow = pkArrow;

        return arrow_count;
    }

    void character::use_arrow(item *arrow, uint32_t arrow_count) {
        int iCount = arrow->count();
        uint32_t dwVnum = arrow->vnum();
        iCount = iCount - std::min<int>(iCount, arrow_count);
        arrow->set_count(iCount);

        if (iCount == 0) {
            item *pkNewArrow = find_specify_item(dwVnum);

            metin2::log::debug("Arrow stack {:d} used up, next stack {}", dwVnum, static_cast<const void *>(pkNewArrow));

            if (pkNewArrow)
                equip_item(pkNewArrow);
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

            character *pkVictim = character_manager::instance().find(target_vid);

            if (!pkVictim)
                return;

            if (!battle_is_attackable(this, pkVictim))
                return;

            if (is_npc()) {
                if (distance_approx(x() - pkVictim->x(), y() - pkVictim->y()) > 5000)
                    return;
            }

            item *pkBow;
            item *pkArrow;

            switch (type) {
            case 0:
                {
                    int iDam = 0;

                    if (is_pc()) {
                        if (job() != JOB_ASSASSIN)
                            return;

                        if (0 == arrow_and_bow(&pkBow, &pkArrow))
                            return;

                        if (skill_group() != 0)
                            if (!is_npc() && skill_group() != 2) {
                                if (sp() < 5)
                                    return;

                                point_change(POINT_SP, -5);
                            }

                        iDam = calc_arrow_damage(this, pkVictim, pkBow, pkArrow);
                        use_arrow(pkArrow, 1);

                        uint32_t dwCurrentTime = get_dword_time();
                        if (is_speed_hack(this, pkVictim, dwCurrentTime))
                            iDam = 0;
                    } else
                        iDam = calc_melee_damage(this, pkVictim);

                    normal_attack_affect(this, pkVictim);

                    iDam = iDam * (100 - pkVictim->point(POINT_RESIST_BOW)) / 100;

                    on_move(true);
                    pkVictim->on_move();

                    if (pkVictim->can_begin_fight())
                        pkVictim->begin_fight(this);

                    pkVictim->damage(this, iDam, DAMAGE_TYPE_NORMAL_RANGE);
                }
                break;

            case 1:
                {
                    int iDam;

                    if (is_pc())
                        return;

                    iDam = calc_magic_damage(this, pkVictim);

                    normal_attack_affect(this, pkVictim);

                    iDam = iDam * (100 - pkVictim->point(POINT_RESIST_MAGIC)) / 100;

                    on_move(true);
                    pkVictim->on_move();

                    if (pkVictim->can_begin_fight())
                        pkVictim->begin_fight(this);

                    pkVictim->damage(this, iDam, DAMAGE_TYPE_MAGIC);
                }
                break;

            case SKILL_YEONSA:
                {
                    int iUseArrow = 1;

                    {
                        if (iUseArrow == arrow_and_bow(&pkBow, &pkArrow, iUseArrow)) {
                            on_move(true);
                            pkVictim->on_move();

                            if (pkVictim->can_begin_fight())
                                pkVictim->begin_fight(this);

                            compute_skill(type, pkVictim);
                            use_arrow(pkArrow, iUseArrow);

                            if (pkVictim->is_dead())
                                break;

                        } else
                            break;
                    }
                }
                break;

            case SKILL_KWANKYEOK:
                {
                    int iUseArrow = 1;

                    if (iUseArrow == arrow_and_bow(&pkBow, &pkArrow, iUseArrow)) {
                        on_move(true);
                        pkVictim->on_move();

                        if (pkVictim->can_begin_fight())
                            pkVictim->begin_fight(this);

                        metin2::log::debug("{} hits {} with kwankeyok", name(), pkVictim->name());
                        compute_skill(type, pkVictim);
                        use_arrow(pkArrow, iUseArrow);
                    }
                }
                break;

            case SKILL_GIGUNG:
                {
                    int iUseArrow = 1;
                    if (iUseArrow == arrow_and_bow(&pkBow, &pkArrow, iUseArrow)) {
                        on_move(true);
                        pkVictim->on_move();

                        if (pkVictim->can_begin_fight())
                            pkVictim->begin_fight(this);

                        metin2::log::debug("{} hits {} with gigung", name(), pkVictim->name());
                        compute_skill(type, pkVictim);
                        use_arrow(pkArrow, iUseArrow);
                    }
                }

                break;
            case SKILL_HWAJO:
                {
                    int iUseArrow = 1;
                    if (iUseArrow == arrow_and_bow(&pkBow, &pkArrow, iUseArrow)) {
                        on_move(true);
                        pkVictim->on_move();

                        if (pkVictim->can_begin_fight())
                            pkVictim->begin_fight(this);

                        metin2::log::debug("{} hits {} with hwajo", name(), pkVictim->name());
                        compute_skill(type, pkVictim);
                        use_arrow(pkArrow, iUseArrow);
                    }
                }

                break;

            case SKILL_HORSE_WILDATTACK_RANGE:
                {
                    int iUseArrow = 1;
                    if (iUseArrow == arrow_and_bow(&pkBow, &pkArrow, iUseArrow)) {
                        on_move(true);
                        pkVictim->on_move();

                        if (pkVictim->can_begin_fight())
                            pkVictim->begin_fight(this);

                        metin2::log::debug("{} hits {} with horse wild attack", name(), pkVictim->name());
                        compute_skill(type, pkVictim);
                        use_arrow(pkArrow, iUseArrow);
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
                    pkVictim->on_move();

                    if (pkVictim->can_begin_fight())
                        pkVictim->begin_fight(this);

                    metin2::log::debug("{}: skill {:d} hits {}", name(), type, pkVictim->name());
                    compute_skill(type, pkVictim);
                }
                break;

            case SKILL_CHAIN:
                {
                    on_move(true);
                    pkVictim->on_move();

                    if (pkVictim->can_begin_fight())
                        pkVictim->begin_fight(this);

                    metin2::log::debug("{}: skill {:d} hits {}", name(), type, pkVictim->name());
                    compute_skill(type, pkVictim);
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
        character *pkVictim = character_manager::instance().find(target_vid_value);
        packet_gc_fly_targeting pack;

        pack.header = (header_value == HEADER_CG_FLY_TARGETING) ? HEADER_GC_FLY_TARGETING : HEADER_GC_ADD_FLY_TARGETING;
        pack.shooter_vid = get_vid();

        if (pkVictim) {
            pack.target_vid = pkVictim->get_vid();
            pack.x = pkVictim->x();
            pack.y = pkVictim->y();

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

        float fMinDist = 99999.0f;
        character *pkVictim = nullptr;

        damage_map::iterator it = map_damage_.begin();

        while (it != map_damage_.end()) {
            const vid &c_VID = it->first;
            ++it;

            character *pAttacker = character_manager::instance().find(c_VID);

            if (!pAttacker)
                continue;

            if (pAttacker->is_affect_flag(AFF_EUNHYUNG) ||
                pAttacker->is_affect_flag(AFF_INVISIBILITY) ||
                pAttacker->is_affect_flag(AFF_REVIVE_INVISIBLE))
                continue;

            float fDist = distance_approx(pAttacker->x() - chr->x(), pAttacker->y() - chr->y());

            if (fDist < fMinDist) {
                pkVictim = pAttacker;
                fMinDist = fDist;
            }
        }

        return pkVictim;
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
        return character_manager::instance().find(vid_victim);
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
        bool bShow = false;

        if (alignment_ == real_alignment_)
            bShow = true;

        int i = alignment_ / 10;

        real_alignment_ = std::clamp(real_alignment_ + amount, -200000, 200000);

        if (bShow) {
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

        int iKillerSeconds = 30;

        if (thecore_pulse() - killer_mode_pulse_ >= passes_per_sec(iKillerSeconds))
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
        sectree *pSec = get_sectree();
        if (pSec) {
            pSec->for_each_around([&](entity *ent) {
                if (ent->is_type(ENTITY_CHARACTER)) {
                    character *ch_value = (character *)ent;
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
        sectree *pSec = get_sectree();
        if (pSec) {
            pSec->for_each_around([&](entity *ent) {
                if (ent->is_type(ENTITY_CHARACTER)) {
                    character *ch_value = (character *)ent;
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
        sectree *pSec = get_sectree();
        if (pSec) {
            pSec->for_each_around([&](entity *ent) {
                if (ent->is_type(ENTITY_CHARACTER)) {
                    character *ch_value = (character *)ent;
                    if (ch_value->is_pc())
                        return;
                    if (!ch_value->is_monster())
                        return;
                    if (ch_value->victim() && ch_value->victim() != this)
                        return;
                    if (ch_value->mob_attack_range() > 150) {
                        int iNewRange = 150;
                        if (iNewRange < 150)
                            iNewRange = 150;

                        ch_value->add_affect(AFFECT_BOW_DISTANCE, POINT_BOW_DISTANCE, iNewRange - ch_value->mob_attack_range(), AFF_NONE, 3 * 60, 0, false);
                    }
                }
            });
        }
    }

    void character::pull_monster() {
        sectree *pSec = get_sectree();
        if (pSec) {
            const int length = 300;
            pSec->for_each_around([&](entity *ent) {
                if (ent->is_type(ENTITY_CHARACTER)) {
                    character *ch_value = (character *)ent;
                    if (ch_value->is_pc())
                        return;
                    if (!ch_value->is_monster())
                        return;
                    float fDist = distance_approx(x() - ch_value->x(), y() - ch_value->y());
                    if (fDist > 3000 || fDist < 100)
                        return;

                    float fNewDist = fDist - length;
                    if (fNewDist < 100)
                        fNewDist = 100;

                    float degree = get_degree_from_position_xy(ch_value->x(), ch_value->y(), x(), y());
                    float fx;
                    float fy;

                    get_delta_by_degree(degree, fDist - fNewDist, &fx, &fy);
                    int32_t tx = (int32_t)(ch_value->x() + fx);
                    int32_t ty = (int32_t)(ch_value->y() + fy);

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
            party *pParty = get_party();

            int iPartyAggroDist = dam;

            if (pParty->leader_pid() == get_vid())
                iPartyAggroDist /= 2;
            else
                iPartyAggroDist /= 3;

            pParty->send_message(this, PM_AGGRO_INCREASE, iPartyAggroDist, attacker->get_vid());
        }

        change_victim_by_aggro(info.aggro, attacker);
    }

    void character::update_aggr_point(character *attacker, damage_type type, int dam) {
        if (is_dead() || is_stun())
            return;

        damage_map::iterator it = map_damage_.find(attacker->get_vid());

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
            damage_map::iterator itFind = map_damage_.end();

            for (it = map_damage_.begin(); it != map_damage_.end(); ++it) {
                if (it->second.aggro > new_aggro) {
                    character *ch = character_manager::instance().find(it->first);

                    if (ch && !ch->is_dead() && distance_approx(ch->x() - x(), ch->y() - y()) < 5000) {
                        itFind = it;
                        new_aggro = it->second.aggro;
                    }
                }
            }

            if (itFind != map_damage_.end()) {
                max_aggro_ = new_aggro;
                set_victim(character_manager::instance().find(itFind->first));
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
