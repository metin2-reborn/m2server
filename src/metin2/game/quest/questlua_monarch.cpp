#include <metin2/config/config.h>
#include <metin2/game/admin/cmd.h>
#include <metin2/game/app/config.h>
#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/characters/mob_manager.h>
#include <metin2/game/empire/castle.h>
#include <metin2/game/empire/monarch.h>
#include <metin2/game/empire/start_position.h>
#include <metin2/game/guilds/guild.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/protocol/p2p.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/quest/lua_incl.h>
#include <metin2/game/quest/questlua.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/world/sectree_manager.h>

#include <metin2/log/log.h>
#include <metin2/proto/character_types.h>
#include <metin2/proto/chat_types.h>
#include <metin2/proto/tables.h>

#include <cstdint>
#include <cstdio>
#include <string>

namespace metin2::game {

    namespace quest {
        EVENTINFO(monarch_powerup_event_info) {
            int empire_index;

            monarch_powerup_event_info()
                : empire_index(0) {
            }
        };

        EVENTINFO(monarch_transfer2_event_info) {
            uint8_t header;
            uint32_t target_pid;
            int32_t x;
            int32_t y;

            monarch_transfer2_event_info()
                : header(0), target_pid(0), x(0), y(0) {
            }
        };

        EVENTFUNC(monarch_powerup_event) {
            auto *info = dynamic_cast<monarch_powerup_event_info *>(event->info.get());

            if (info == nullptr) {
                quest::error("Monarch power-up event has no info");
                return 0;
            }

            get_monarch().power_up(info->empire_index, false);
            return 0;
        }

        EVENTINFO(monarch_defenseup_event_info) {
            int empire_index;

            monarch_defenseup_event_info()
                : empire_index(0) {
            }
        };

        EVENTFUNC(monarch_defenseup_event) {
            auto *info = dynamic_cast<monarch_powerup_event_info *>(event->info.get());

            if (info == nullptr) {
                quest::error("Monarch defense-up event has no info");
                return 0;
            }

            get_monarch().defense_up(info->empire_index, false);
            return 0;
        }

        auto takemonarchmoney(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();

            int money = (int)lua_tonumber(lua, 1);
            int pid_value = ch->player_id();
            int empire_value = ch->empire();
            money = money * 10000;

            metin2::log::info("Monarch: pid {1:d} takes {2:d} gold from empire {0:d}", ch->empire(), ch->player_id(), money);

            db_clientdesc->db_packet_header(HEADER_GD_TAKE_MONARCH_MONEY, ch->get_desc()->handle(), sizeof(int) * 3);
            db_clientdesc->packet(&empire_value, sizeof(int));
            db_clientdesc->packet(&pid_value, sizeof(int));
            db_clientdesc->packet(&money, sizeof(int));
            return 1;
        }

        auto is_guild_master(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();

            if (ch->get_guild()) {
                guild_member *member = ch->get_guild()->member(ch->player_id());

                if (member) {
                    if (member->grade <= 4) {
                        lua_pushnumber(lua, 1);
                        return 1;
                    }
                }
            }
            lua_pushnumber(lua, 0);

            return 1;
        }

        auto monarch_bless(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();

            if (false == ch->is_monarch()) {
                if (!ch->is_gm()) {
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("군주의 자격을 가지고 있지 않습니다"));
                    quest::error("Player (pid {:d}) is not a monarch", ch->player_id());
                    return 0;
                }
            }

            int heal_price = quest::get_quest_manager().event_flag("MonarchHealGold");
            if (heal_price == 0)
                heal_price = 2000000;

            if (get_monarch().heal_my_empire(ch, heal_price)) {
                char notice[256];
                snprintf(notice, sizeof(notice),
                    lc_text("군주의 축복으로 이지역 %s 유저는 HP,SP가 모두 채워집니다."), empire_name(ch->empire()));
                send_notice_map(notice, ch->map_index(), false);

                ch->chat_packet(CHAT_TYPE_INFO, lc_text("군주의 축복을 사용하였습니다."));
            }

            return 1;
        }

        auto monarch_powerup(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();

            if (!ch)
                return 0;

            if (false == ch->is_monarch()) {
                if (!ch->is_gm()) {
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("군주의 자격을 가지고 있지 않습니다"));
                    quest::error("Player (pid {:d}) is not a monarch", ch->player_id());
                    return 0;
                }
            }

            int money_need = 5000000;
            if (!get_monarch().is_money_ok(money_need, ch->empire())) {
                int nation_money = get_monarch().money(ch->empire());
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("국고에 돈이 부족합니다. 현재 : %u 필요금액 : %u"), nation_money, money_need);
                return 0;
            }

            if (!get_monarch().check_power_up_ct(ch->empire())) {
                int next_sec = get_monarch().power_up_ct(ch->empire()) / metin2::config::server().passes_per_sec;
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("쿨타임 적용중  %d 후 사용가능"), next_sec);
                return 0;
            }

            get_monarch().power_up(ch->empire(), true);

            int monarch_power_up_ct = 60 * 3;

            auto *info = alloc_event_info<monarch_powerup_event_info>();

            info->empire_index = ch->empire();

            event_create(quest::monarch_powerup_event, info, passes_per_sec(monarch_power_up_ct));

            get_monarch().sendto_db_dec_money(5000000, ch->empire(), ch);

            char notice[256];
            snprintf(notice, sizeof(notice), lc_text("군주의 사자후 영향으로 이지역 %s 유저는 3분간 10 %% 의 공격력이  증가됩니다"), empire_name(ch->empire()));

            send_notice_map(notice, ch->map_index(), false);

            return 1;
        }
        auto monarch_defenseup(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();

            if (!ch)
                return 0;

            if (false == ch->is_monarch()) {
                if (!ch->is_gm()) {
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("군주의 자격을 가지고 있지 않습니다"));
                    quest::error("Player (pid {:d}) is not a monarch", ch->player_id());
                    return 0;
                }
            }

            int money_need = 5000000;
            if (!get_monarch().is_money_ok(money_need, ch->empire())) {
                int nation_money = get_monarch().money(ch->empire());
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("국고에 돈이 부족합니다. 현재 : %u 필요금액 : %u"), nation_money, money_need);
                return 0;
            }

            if (!get_monarch().check_defense_up_ct(ch->empire())) {
                int next_sec = get_monarch().defense_up_ct(ch->empire()) / metin2::config::server().passes_per_sec;
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("쿨타임 적용중  %d 후 사용가능"), next_sec);
                return 0;
            }

            get_monarch().defense_up(ch->empire(), true);

            int monarch_defense_up_ct = 60 * 3;

            auto *info = alloc_event_info<monarch_defenseup_event_info>();

            info->empire_index = ch->empire();

            event_create(quest::monarch_defenseup_event, info, passes_per_sec(monarch_defense_up_ct));

            get_monarch().sendto_db_dec_money(5000000, ch->empire(), ch);

            char notice[256];
            snprintf(notice, sizeof(notice), lc_text("군주의 금강권 영향으로 이지역 %s 유저는 3분간 10 %% 의 방어력이  증가됩니다"), empire_name(ch->empire()));

            send_notice_map(notice, ch->map_index(), false);

            return 1;
        }

        auto is_monarch(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            if (!ch)
                return 0;
            lua_pushnumber(lua, ch->is_monarch());
            return 1;
        }

        auto spawn_mob(lua_State *lua) -> int {
            if (!require_number(lua, 1))
                return 0;

            auto mob_vnum = (uint32_t)lua_tonumber(lua, 1);

            character *ch = get_quest_manager().current_character_ptr();

            if (!ch)
                return 0;

            const mob *mob_value = nullptr;

            if (!(mob_value = get_mob_manager().get(mob_vnum)))
                if (mob_value == nullptr) {
                    ch->chat_packet(CHAT_TYPE_INFO, "No such mob by that vnum");
                    return 0;
                }

            if (false == ch->is_monarch()) {
                if (!ch->is_gm()) {
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("군주의 자격을 가지고 있지 않습니다"));
                    quest::error("Player (pid {:d}) is not a monarch", ch->player_id());
                    return 0;
                }
            }

            uint32_t quest_idx = get_quest_manager().current_pc()->current_quest_index();

            bool ret = false;
            character *mob = nullptr;

            {
                int32_t x = ch->x();
                int32_t y = ch->y();
                {
                    mob = get_character_manager().spawn_mob(mob_vnum, ch->map_index(), x, y, 0, mob_value->table.type == CHAR_TYPE_STONE, -1);
                }

                if (mob) {

                    mob->set_quest_by(quest_idx);

                    if (!ret) {
                        ret = true;
                        lua_pushnumber(lua, (uint32_t)mob->get_vid());
                    }
                }
            }

            return 1;
        }

        auto spawn_guard(lua_State *lua) -> int {
            if (!lua_isnumber(lua, 1) || !lua_isnumber(lua, 2)) {
                quest::error("Invalid arguments");
                return 0;
            }

            auto group_vnum = (uint32_t)lua_tonumber(lua, 1);
            int region_index = (int)lua_tonumber(lua, 2);
            character *ch = get_quest_manager().current_character_ptr();
            if (!ch)
                return 0;

            if (false == ch->is_monarch()) {
                if (!ch->is_gm()) {
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("군주의 자격을 가지고 있지 않습니다"));
                    quest::error("Player (pid {:d}) is not a monarch", ch->player_id());
                    return 0;
                }
            }

            if (false == castle_is_my_castle(ch->empire(), ch->map_index())) {
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("성에서만 사용할 수 있는 기능입니다."));
                return 0;
            }

            character *guard_leader = nullptr;
            {
                int money_need = castle_cost_of_hiring_guard(group_vnum);

                if (!get_monarch().is_money_ok(money_need, ch->empire())) {
                    int nation_money = get_monarch().money(ch->empire());
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("국고에 돈이 부족합니다. 현재 : %u 필요금액 : %u"), nation_money, money_need);
                    return 0;
                }
                guard_leader = castle_spawn_guard(ch->empire(), group_vnum, region_index);

                if (guard_leader) {
                    get_monarch().sendto_db_dec_money(money_need, ch->empire(), ch);
                    castle_save();
                }
            }

            return 1;
        }

        auto monarch_warp(lua_State *lua) -> int {
            if (!require_string(lua, 1))
                return 0;

            std::string name = lua_tostring(lua, 1);

            character *ch = get_quest_manager().current_character_ptr();
            if (!ch)
                return 0;

            if (!get_monarch().is_monarch(ch->player_id(), ch->empire())) {
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("군주만이 사용 가능한 기능입니다"));
                return 0;
            }

            if (!ch->is_mcok(character::MI_WARP)) {
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("%d 초간 쿨타임이 적용중입니다."), ch->mcl_time(character::MI_WARP));
                return 0;
            }

            const int warp_price = 10000;

            if (!get_monarch().is_money_ok(warp_price, ch->empire())) {
                int nation_money = get_monarch().money(ch->empire());
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("국고에 돈이 부족합니다. 현재 : %u 필요금액 : %u"), nation_money, warp_price);
                return 0;
            }

            int x, y;

            character *tch = get_character_manager().find_pc(name.c_str());

            if (!tch) {
                cci *cci = get_p2p_manager().find(name.c_str());

                if (cci) {
                    if (cci->empire != ch->empire()) {
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("타제국 유저에게는 이동할수 없습니다"));
                        return 0;
                    }
                    if (cci->channel != metin2::config::server().channel) {
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("해당 유저는 %d 채널에 있습니다. (현재 채널 %d)"), cci->channel, metin2::config::server().channel);
                        return 0;
                    }

                    if (!is_monarch_warp_zone(cci->map_index)) {
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("해당 지역으로 이동할 수 없습니다."));
                        return 0;
                    }

                    pixel_position pos;

                    if (!get_sectree_manager().center_position_of_map(cci->map_index, pos))
                        ch->chat_packet(CHAT_TYPE_INFO, "Cannot find map (index %d)", cci->map_index);
                    else {
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("%s 에게로 이동합니다"), name.c_str());
                        ch->warp_set(pos.x, pos.y);

                        get_monarch().sendto_db_dec_money(warp_price, ch->empire(), ch);

                        ch->set_mc(character::MI_WARP);
                    }

                } else if (nullptr == get_character_manager().find_pc(name.c_str())) {
                    ch->chat_packet(CHAT_TYPE_INFO, "There is no one by that name");
                }

                return 0;
            } else {
                if (tch->empire() != ch->empire()) {
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("타제국 유저에게는 이동할수 없습니다"));
                    return 0;
                }

                if (!is_monarch_warp_zone(tch->map_index())) {
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("해당 지역으로 이동할 수 없습니다."));
                    return 0;
                }

                x = tch->x();
                y = tch->y();
            }

            ch->chat_packet(CHAT_TYPE_INFO, lc_text("%s 에게로 이동합니다"), name.c_str());
            ch->warp_set(x, y);
            ch->stop();

            get_monarch().sendto_db_dec_money(warp_price, ch->empire(), ch);

            ch->set_mc(character::MI_WARP);

            return 0;
        }

        auto empire_info(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();

            if (nullptr == ch)
                return false;

            monarch_info *p = get_monarch().info();

            if (get_monarch().is_monarch(ch->player_id(), ch->empire())) {
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("나의 군주 정보"));

                for (int n = 1; n < 4; ++n) {
                    if (n == ch->empire())
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("[%s군주] : %s  보유금액 %lld "), empire_name(n), p->name[n], p->money[n]);
                    else
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("[%s군주] : %s  "), empire_name(n), p->name[n]);
                }
            } else {
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("군주 정보"));

                for (int n = 1; n < 4; ++n)
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("[%s군주] : %s  "), empire_name(n), p->name[n]);
            }

            return 0;
        }

        auto monarch_transfer(lua_State *lua) -> int {
            if (!require_string(lua, 1))
                return 0;

            std::string name = lua_tostring(lua, 1);

            character *ch = get_quest_manager().current_character_ptr();

            if (!ch)
                return 0;

            if (!get_monarch().is_monarch(ch->player_id(), ch->empire())) {
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("군주만이 사용 가능한 기능입니다"));
                return 0;
            }

            if (!ch->is_mcok(character::MI_TRANSFER)) {
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("%d 초간 쿨타임이 적용중입니다."), ch->mcl_time(character::MI_TRANSFER));
                return 0;
            }

            const int warp_price = 10000;

            if (!get_monarch().is_money_ok(warp_price, ch->empire())) {
                int nation_money = get_monarch().money(ch->empire());
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("국고에 돈이 부족합니다. 현재 : %u 필요금액 : %u"), nation_money, warp_price);
                return 0;
            }

            character *tch = get_character_manager().find_pc(name.c_str());

            if (!tch) {
                cci *cci = get_p2p_manager().find(name.c_str());

                if (cci) {
                    if (cci->empire != ch->empire()) {
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("다른 제국 유저는 소환할 수 없습니다."));
                        return 0;
                    }

                    if (cci->channel != metin2::config::server().channel) {
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("%s 님은 %d 채널에 접속 중 입니다. (현재 채널: %d)"), name.c_str(), cci->channel, metin2::config::server().channel);
                        return 0;
                    }

                    if (!is_monarch_warp_zone(cci->map_index)) {
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("해당 지역으로 이동할 수 없습니다."));
                        return 0;
                    }
                    if (!is_monarch_warp_zone(ch->map_index())) {
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("해당 지역으로 소환할 수 없습니다."));
                        return 0;
                    }

                    packet_gg_transfer pgg;

                    pgg.header = HEADER_GG_TRANSFER;
                    strlcpy(pgg.name, name.c_str(), sizeof(pgg.name));
                    pgg.x = ch->x();
                    pgg.y = ch->y();

                    get_p2p_manager().send(&pgg, sizeof(packet_gg_transfer));
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("%s 님을 소환하였습니다."), name.c_str());

                    get_monarch().sendto_db_dec_money(warp_price, ch->empire(), ch);

                    ch->set_mc(character::MI_TRANSFER);
                } else {
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("입력하신 이름을 가진 사용자가 없습니다."));
                }

                return 0;
            }

            if (ch == tch) {
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("자신을 소환할 수 없습니다."));
                return 0;
            }

            if (tch->empire() != ch->empire()) {
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("다른 제국 유저는 소환할 수 없습니다."));
                return 0;
            }

            if (!is_monarch_warp_zone(tch->map_index())) {
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("해당 지역으로 이동할 수 없습니다."));
                return 0;
            }
            if (!is_monarch_warp_zone(ch->map_index())) {
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("해당 지역으로 소환할 수 없습니다."));
                return 0;
            }
            tch->warp_set(ch->x(), ch->y(), ch->map_index());

            get_monarch().sendto_db_dec_money(warp_price, ch->empire(), ch);
            ch->set_mc(character::MI_TRANSFER);
            return 0;
        }

        auto monarch_notice(lua_State *lua) -> int {
            if (!lua_isstring(lua, 1))
                return 0;

            character *ch = get_quest_manager().current_character_ptr();

            if (ch == nullptr)
                return 0;

            if (ch->is_monarch() == false) {
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("군주만이 사용 가능한 기능입니다"));
                return 0;
            }

            std::string notice = lua_tostring(lua, 1);

            if (notice.length() > 0)
                send_monarch_notice(ch->empire(), notice.c_str());

            return 0;
        }

        auto monarch_mob(lua_State *lua) -> int {
            if (!lua_isstring(lua, 1))
                return 0;

            character *ch = get_quest_manager().current_character_ptr();

            if (ch == nullptr)
                return 0;

            std::string vnum(lua_tostring(lua, 1));
            do_monarch_mob(ch, vnum.c_str(), 0, 0);
            return 0;
        }

        EVENTFUNC(monarch_transfer2_event) {
            auto *info = dynamic_cast<monarch_transfer2_event_info *>(event->info.get());

            if (info == nullptr) {
                quest::error("Monarch transfer event has no info");
                return 0;
            }

            character *target_char = get_character_manager().find_by_pid(info->target_pid);

            if (target_char != nullptr) {
                unsigned int q_index = quest::get_quest_manager().quest_index_by_name("monarch_transfer");

                if (q_index != 0) {
                    target_char->set_quest_flag("monarch_transfer.x", info->x);
                    target_char->set_quest_flag("monarch_transfer.y", info->y);
                    quest::get_quest_manager().letter(target_char->player_id(), q_index, 0);
                }
            }

            return 0;
        }

        auto monarch_transfer2(lua_State *lua) -> int {
            if (lua_isstring(lua, 1) == false) return 0;

            character *ch = get_quest_manager().current_character_ptr();
            if (ch == nullptr) return false;

            if (get_monarch().is_monarch(ch->player_id(), ch->empire()) == false) {
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("군주만이 사용 가능한 기능입니다"));
                return 0;
            }

            if (ch->is_mcok(character::MI_TRANSFER) == false) {
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("%d 초간 쿨타임이 적용중입니다."), ch->mcl_time(character::MI_TRANSFER));
                return 0;
            }

            const int ci_transfer_cost = 10000;

            if (get_monarch().is_money_ok(ci_transfer_cost, ch->empire()) == false) {
                int nation_money = get_monarch().money(ch->empire());
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("국고에 돈이 부족합니다. 현재 : %u 필요금액 : %u"), nation_money, ci_transfer_cost);
                return 0;
            }

            std::string target_name = lua_tostring(lua, 1);

            character *target_char = get_character_manager().find_pc(target_name.c_str());

            if (target_char == nullptr) {
                cci *cci = get_p2p_manager().find(target_name.c_str());

                if (cci != nullptr) {
                    if (cci->empire != ch->empire()) {
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("다른 제국 유저는 소환할 수 없습니다."));
                        return 0;
                    }

                    if (cci->channel != metin2::config::server().channel) {
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("%s 님은 %d 채널에 접속중입니다. (현재 채널: %d)"),
                            target_name.c_str(), cci->channel, metin2::config::server().channel);
                        return 0;
                    }

                    if (!is_monarch_warp_zone(cci->map_index)) {
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("해당 지역으로 이동할 수 없습니다."));
                        return 0;
                    }
                    if (!is_monarch_warp_zone(ch->map_index())) {
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("해당 지역으로 소환할 수 없습니다."));
                        return 0;
                    }

                    packet_monarch_gg_transfer packet;
                    packet.header = HEADER_GG_MONARCH_TRANSFER;
                    packet.target_pid = cci->pid;
                    packet.x = ch->x();
                    packet.y = ch->y();

                    get_p2p_manager().send(&packet, sizeof(packet_monarch_gg_transfer));
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("소환 요청을 보냈습니다"));

                    get_monarch().sendto_db_dec_money(ci_transfer_cost, ch->empire(), ch);
                    ch->set_mc(character::MI_TRANSFER);
                } else {
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("입력하신 이름을 가진 사용자가 없습니다."));
                    return 0;
                }
            } else {
                if (target_char == ch) {
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("자신을 소환할 수 없습니다."));
                    return 0;
                }

                if (target_char->empire() != ch->empire()) {
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("다른 제국 유저는 소환할 수 없습니다."));
                    return 0;
                }

                if (distance_approx(target_char->x() - ch->x(), target_char->y() - ch->y()) <= 5000) {
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("%s 님은 근처에 있습니다"), target_char->name());
                    return 0;
                }

                if (!is_monarch_warp_zone(target_char->map_index())) {
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("해당 지역으로 이동할 수 없습니다."));
                    return 0;
                }
                if (!is_monarch_warp_zone(ch->map_index())) {
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("해당 지역으로 소환할 수 없습니다."));
                    return 0;
                }

                auto *info = alloc_event_info<monarch_transfer2_event_info>();

                info->header = HEADER_GG_MONARCH_TRANSFER;
                info->target_pid = target_char->player_id();
                info->x = ch->x();
                info->y = ch->y();

                event_create(monarch_transfer2_event, info, 1);

                ch->chat_packet(CHAT_TYPE_INFO, lc_text("소환 요청을 보냈습니다"));

                get_monarch().sendto_db_dec_money(ci_transfer_cost, ch->empire(), ch);
                ch->set_mc(character::MI_TRANSFER);
                return 0;
            }

            return 0;
        }

        void register_monarch_function_table() {
            luaL_reg monarch_functions[] =
                {
                    {"takemonarchmoney", takemonarchmoney },
                    {"isguildmaster",    is_guild_master  },
                    {"ismonarch",        is_monarch       },
                    {"monarchbless",     monarch_bless    },
                    {"monarchpowerup",   monarch_powerup  },
                    {"monarchdefenseup", monarch_defenseup},
                    {"spawnmob",         spawn_mob        },
                    {"spawnguard",       spawn_guard      },
                    {"warp",             monarch_warp     },
                    {"transfer",         monarch_transfer },
                    {"transfer2",        monarch_transfer2},
                    {"info",             empire_info      },
                    {"notice",           monarch_notice   },
                    {"monarch_mob",      monarch_mob      },

                    {nullptr,            nullptr          }
            };

            get_quest_manager().add_lua_function_table("oh", monarch_functions);
        }

    }
}
