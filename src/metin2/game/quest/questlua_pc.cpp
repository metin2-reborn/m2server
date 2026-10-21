

#include <algorithm>
#include <metin2/config/config.h>
#include <metin2/game/app/constants.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/characters/affect.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/characters/mob_manager.h>
#include <metin2/game/characters/polymorph.h>
#include <metin2/game/empire/start_position.h>
#include <metin2/game/events/mining.h>
#include <metin2/game/guilds/guild_manager.h>
#include <metin2/game/guilds/war_map.h>
#include <metin2/game/items/item.h>
#include <metin2/game/items/item_manager.h>
#include <metin2/game/items/unique_item.h>
#include <metin2/game/persistence/db.h>
#include <metin2/game/persistence/log.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/protocol/p2p.h>
#include <metin2/game/quest/lua_incl.h>
#include <metin2/game/quest/questlua.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/quest/questpc.h>
#include <metin2/game/social/marriage.h>
#include <metin2/game/social/messenger_manager.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/text/locale_service.h>
#include <metin2/game/world/sectree_manager.h>

#include <metin2/common/utils.h>
#include <metin2/core/utils.h>
#include <metin2/log/log.h>
#include <metin2/proto/character_types.h>
#include <metin2/proto/chat_types.h>
#include <metin2/proto/inventory.h>
#include <metin2/proto/item_types.h>
#include <metin2/proto/log_types.h>
#include <metin2/proto/tables.h>
#include <metin2/sql/async_sql.h>
#include <metin2/sql/query.h>

#include <mysql.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <format>
#include <memory>
#include <string>
#include <vector>

namespace metin2::game {

    const int ITEM_BROKEN_METIN_VNUM = 28960;

    namespace quest {
        auto pc_has_master_skill(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            bool has_master_skill = false;
            for (int i = 0; i < SKILL_MAX_NUM; i++)
                if (ch->skill_master_type(i) >= SKILL_MASTER && ch->skill_level(i) >= 21) {
                    has_master_skill = true;
                    break;
                }

            lua_pushboolean(lua, has_master_skill);
            return 1;
        }

        auto pc_remove_skill_book_no_delay(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            ch->remove_affect(AFFECT_SKILL_NO_BOOK_DELAY);
            return 0;
        }

        auto pc_is_skill_book_no_delay(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();

            lua_pushboolean(lua, ch->find_affect(AFFECT_SKILL_NO_BOOK_DELAY) ? true : false);
            return 1;
        }

        auto pc_learn_grand_master_skill(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();

            if (!require_number(lua, 1, "Invalid skill index argument"))
                return 0;

            lua_pushboolean(lua, ch->learn_grand_master_skill((int32_t)lua_tonumber(lua, 1)));
            return 1;
        }

        auto pc_set_warp_location(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();

            if (!require_number(lua, 1, "Invalid map index argument"))
                return 0;

            if (!lua_isnumber(lua, 2) || !lua_isnumber(lua, 3)) {
                quest::error("Invalid coordinate arguments");
                return 0;
            }

            ch->set_warp_location((int32_t)lua_tonumber(lua, 1), (int32_t)lua_tonumber(lua, 2), (int32_t)lua_tonumber(lua, 3));
            return 0;
        }

        auto pc_set_warp_location_local(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();

            if (!require_number(lua, 1, "Invalid map index argument"))
                return 0;

            if (!lua_isnumber(lua, 2) || !lua_isnumber(lua, 3)) {
                quest::error("Invalid coordinate arguments");
                return 0;
            }

            auto map_index = (int32_t)lua_tonumber(lua, 1);
            const map_region *region = get_sectree_manager().get_map_region(map_index);

            if (!region) {
                quest::error("No map with index {:d}", map_index);
                return 0;
            }

            int x = (int)lua_tonumber(lua, 2);
            int y = (int)lua_tonumber(lua, 3);

            if (x > region->ex - region->sx) {
                quest::error("X coordinate {1:d} exceeds map width {0:d}", region->ex - region->sx, x);
                return 0;
            }

            if (y > region->ey - region->sy) {
                quest::error("Y coordinate {1:d} exceeds map height {0:d}", region->ey - region->sy, y);
                return 0;
            }

            ch->set_warp_location(map_index, x, y);
            return 0;
        }

        auto pc_get_start_location(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();

            lua_pushnumber(lua, start_map[ch->empire()]);
            lua_pushnumber(lua, start_position[ch->empire()][0] / 100);
            lua_pushnumber(lua, start_position[ch->empire()][1] / 100);
            return 3;
        }

        auto pc_warp(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();

            if (!lua_isnumber(lua, 1) || !lua_isnumber(lua, 2)) {
                lua_pushboolean(lua, false);
                return 1;
            }

            int32_t map_index = 0;

            if (lua_isnumber(lua, 3))
                map_index = (int)lua_tonumber(lua, 3);

            if (ch->is_hack()) {
                lua_pushboolean(lua, false);
                return 1;
            }

            if (metin2::config::test_server())
                ch->chat_packet(CHAT_TYPE_INFO, "pc_warp %d %d %d", (int)lua_tonumber(lua, 1),
                    (int)lua_tonumber(lua, 2), map_index);
            ch->warp_set((int)lua_tonumber(lua, 1), (int)lua_tonumber(lua, 2), map_index);

            lua_pushboolean(lua, true);

            return 1;
        }

        auto pc_warp_local(lua_State *lua) -> int {
            if (!require_number(lua, 1, "Missing map index argument"))
                return 0;

            if (!lua_isnumber(lua, 2) || !lua_isnumber(lua, 3)) {
                quest::error("Missing coordinate arguments");
                return 0;
            }

            auto map_index = (int32_t)lua_tonumber(lua, 1);
            const map_region *region = get_sectree_manager().get_map_region(map_index);

            if (!region) {
                quest::error("No map with index {:d}", map_index);
                return 0;
            }

            int x = (int)lua_tonumber(lua, 2);
            int y = (int)lua_tonumber(lua, 3);

            if (x > region->ex - region->sx) {
                quest::error("X coordinate {1:d} exceeds map width {0:d}", region->ex - region->sx, x);
                return 0;
            }

            if (y > region->ey - region->sy) {
                quest::error("Y coordinate {1:d} exceeds map height {0:d}", region->ey - region->sy, y);
                return 0;
            }

            get_quest_manager().current_character_ptr()->warp_set(region->sx + x, region->sy + y);
            return 0;
        }

        auto pc_warp_exit(lua_State *lua) -> int {
            get_quest_manager().current_character_ptr()->exit_to_saved_location();
            return 0;
        }

        auto pc_in_dungeon(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            lua_pushboolean(lua, ch->get_dungeon() ? 1 : 0);
            return 1;
        }

        auto pc_hasguild(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            lua_pushboolean(lua, ch->get_guild() ? 1 : 0);
            return 1;
        }

        auto pc_getguild(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            lua_pushnumber(lua, ch->get_guild() ? ch->get_guild()->id() : 0);
            return 1;
        }

        auto pc_isguildmaster(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            guild *g = ch->get_guild();

            if (g)
                lua_pushboolean(lua, (ch->player_id() == g->master_pid()));
            else
                lua_pushboolean(lua, 0);

            return 1;
        }

        auto pc_destroy_guild(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            guild *g = ch->get_guild();

            if (g)
                g->request_disband(ch->player_id());

            return 0;
        }

        auto pc_remove_from_guild(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            guild *g = ch->get_guild();

            if (g)
                g->request_remove_member(ch->player_id());

            return 0;
        }

        auto pc_give_gold(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();

            if (!require_number(lua, 1))
                return 0;

            int amount_value = (int)lua_tonumber(lua, 1);

            if (amount_value <= 0) {
                quest::error("Gold amount {:d} is not positive", amount_value);
                return 0;
            }

            get_db_manager().send_money_log(MONEY_LOG_QUEST, ch->player_id(), amount_value);
            ch->point_change(POINT_GOLD, amount_value, true);
            return 0;
        }

        auto pc_warp_to_guild_war_observer_position(lua_State *lua) -> int {
            luaL_checknumber(lua, 1);
            luaL_checknumber(lua, 2);

            auto gid1 = (uint32_t)lua_tonumber(lua, 1);
            auto gid2 = (uint32_t)lua_tonumber(lua, 2);

            guild *g1 = get_guild_manager().find_guild(gid1);
            guild *g2 = get_guild_manager().find_guild(gid2);

            if (!g1 || !g2) {
                luaL_error(lua, "no such guild with id %d %d", gid1, gid2);
            }

            pixel_position pos;

            uint32_t map_index = g1->guild_war_map_index(gid2);

            if (!get_war_map_manager().start_position(map_index, 2, pos)) {
                luaL_error(lua, "not under warp guild war between guild %d %d", gid1, gid2);
                return 0;
            }

            character *ch = get_quest_manager().current_character_ptr();

            if (ch->is_hack())
                return 0;

            ch->set_quest_flag("war.is_war_member", 0);
            ch->save_exit_location();
            ch->warp_set(pos.x, pos.y, map_index);
            return 0;
        }

        auto pc_give_item_from_special_item_group(lua_State *lua) -> int {
            luaL_checknumber(lua, 1);

            character *ch = get_quest_manager().current_character_ptr();

            auto group_vnum = (uint32_t)lua_tonumber(lua, 1);

            std::vector<uint32_t> vnums;
            std::vector<uint32_t> counts;
            std::vector<item *> item_gets;
            int count = 0;

            ch->give_item_from_special_item_group(group_vnum, vnums, counts, item_gets, count);

            for (int i = 0; i < count; i++) {
                if (!item_gets[i]) {
                    if (vnums[i] == 1) {
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("돈 %d 냥을 획득했습니다."), counts[i]);
                    } else if (vnums[i] == 2) {
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("나무에서 부터 신비한 빛이 나옵니다."));
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("%d의 경험치를 획득했습니다."), counts[i]);
                    }
                }
            }
            return 0;
        }

        auto pc_enough_inventory(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            if (!lua_isnumber(lua, 1)) {
                lua_pushboolean(lua, 0);
                return 1;
            }

            auto item_vnum = (uint32_t)lua_tonumber(lua, 1);
            item_table *table = get_item_manager().table(item_vnum);
            if (!table) {
                lua_pushboolean(lua, 0);
                return 1;
            }

            bool enough_inventory_for_item = ch->empty_inventory(table->size) != -1;
            lua_pushboolean(lua, enough_inventory_for_item);
            return 1;
        }

        auto pc_give_item(lua_State *lua) -> int {
            pc *pc = get_quest_manager().current_pc();
            character *ch = get_quest_manager().current_character_ptr();

            if (!lua_isstring(lua, 1) || !(lua_isstring(lua, 2) || lua_isnumber(lua, 2))) {
                quest::error("Invalid arguments");
                return 0;
            }

            uint32_t vnum;

            if (lua_isnumber(lua, 2))
                vnum = (int)lua_tonumber(lua, 2);
            else if (!get_item_manager().vnum(lua_tostring(lua, 2), vnum)) {
                quest::error("Unknown item name {}", lua_tostring(lua, 1));
                return 0;
            }

            int icount = 1;

            if (lua_isnumber(lua, 3) && lua_tonumber(lua, 3) > 0) {
                icount = (int)rint(lua_tonumber(lua, 3));

                if (icount <= 0) {
                    quest::error("Invalid item count {:g}", lua_tonumber(lua, 2));
                    return 0;
                }
            }

            pc->give_item(lua_tostring(lua, 1), vnum, icount);

            get_log_manager().quest_reward_log(pc->current_quest_name().c_str(), ch->player_id(), ch->level(), vnum, icount);
            return 0;
        }

        auto pc_give_or_drop_item(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();

            if (!lua_isstring(lua, 1) && !lua_isnumber(lua, 1)) {
                quest::error("Invalid item argument");
                lua_pushnumber(lua, 0);
                return 1;
            }

            uint32_t vnum;

            if (lua_isnumber(lua, 1)) {
                vnum = (int)lua_tonumber(lua, 1);
            } else if (!get_item_manager().vnum(lua_tostring(lua, 1), vnum)) {
                quest::error("Unknown item name {}", lua_tostring(lua, 1));
                lua_pushnumber(lua, 0);

                return 1;
            }

            int icount = 1;
            if (lua_isnumber(lua, 2) && lua_tonumber(lua, 2) > 0) {
                icount = (int)rint(lua_tonumber(lua, 2));
                if (icount <= 0) {
                    quest::error("Invalid item count {:g}", lua_tonumber(lua, 2));
                    lua_pushnumber(lua, 0);
                    return 1;
                }
            }

            metin2::log::info("Quest reward: item {} to player {}", lua_tostring(lua, 1), ch->name());

            pc *pc = get_quest_manager().current_pc();

            get_log_manager().quest_reward_log(pc->current_quest_name().c_str(), ch->player_id(), ch->level(), vnum, icount);

            item *item = ch->auto_give_item(vnum, icount);

            if (vnum >= 80003 && vnum <= 80007) {
                get_log_manager().gold_bar_log(ch->player_id(), item->id(), QUEST, "quest: give_item2");
            }

            if (nullptr != item)
                lua_pushnumber(lua, item->id());
            else
                lua_pushnumber(lua, 0);
            return 1;
        }

        auto pc_give_or_drop_item_and_select(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();

            if (!lua_isstring(lua, 1) && !lua_isnumber(lua, 1)) {
                quest::error("Invalid item argument");
                return 0;
            }

            uint32_t vnum;

            if (lua_isnumber(lua, 1)) {
                vnum = (int)lua_tonumber(lua, 1);
            } else if (!get_item_manager().vnum(lua_tostring(lua, 1), vnum)) {
                quest::error("Unknown item name {}", lua_tostring(lua, 1));
                return 0;
            }

            int icount = 1;
            if (lua_isnumber(lua, 2) && lua_tonumber(lua, 2) > 0) {
                icount = (int)rint(lua_tonumber(lua, 2));
                if (icount <= 0) {
                    quest::error("Invalid item count {:g}", lua_tonumber(lua, 2));
                    return 0;
                }
            }

            metin2::log::info("Quest reward: item {} to player {}", lua_tostring(lua, 1), ch->name());

            pc *pc = get_quest_manager().current_pc();

            get_log_manager().quest_reward_log(pc->current_quest_name().c_str(), ch->player_id(), ch->level(), vnum, icount);

            item *item = ch->auto_give_item(vnum, icount);

            if (nullptr != item)
                get_quest_manager().set_current_item(item);

            if (vnum >= 80003 && vnum <= 80007) {
                get_log_manager().gold_bar_log(ch->player_id(), item->id(), QUEST, "quest: give_item2");
            }

            return 0;
        }

        auto pc_get_current_map_index(lua_State *lua) -> int {
            lua_pushnumber(lua, get_quest_manager().current_character_ptr()->map_index());
            return 1;
        }

        auto pc_get_x(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            lua_pushnumber(lua, ch->x() / 100);
            return 1;
        }

        auto pc_get_y(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            lua_pushnumber(lua, ch->y() / 100);
            return 1;
        }

        auto pc_get_local_x(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            sectree_map *map = get_sectree_manager().map(ch->map_index());

            if (map)
                lua_pushnumber(lua, (ch->x() - map->setting.base_x) / 100);
            else
                lua_pushnumber(lua, ch->x() / 100);

            return 1;
        }

        auto pc_get_local_y(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            sectree_map *map = get_sectree_manager().map(ch->map_index());

            if (map)
                lua_pushnumber(lua, (ch->y() - map->setting.base_y) / 100);
            else
                lua_pushnumber(lua, ch->y() / 100);

            return 1;
        }

        auto pc_count_item(lua_State *lua) -> int {
            if (lua_isnumber(lua, -1))
                lua_pushnumber(lua, get_quest_manager().current_character_ptr()->count_specify_item((uint32_t)lua_tonumber(lua, -1)));
            else if (lua_isstring(lua, -1)) {
                uint32_t item_vnum;

                if (!get_item_manager().vnum(lua_tostring(lua, 1), item_vnum)) {
                    quest::error("Unknown item name {}", lua_tostring(lua, 1));
                    lua_pushnumber(lua, 0);
                } else {
                    lua_pushnumber(lua, get_quest_manager().current_character_ptr()->count_specify_item(item_vnum));
                }
            } else
                lua_pushnumber(lua, 0);

            return 1;
        }

        auto pc_remove_item(lua_State *lua) -> int {
            if (lua_gettop(lua) == 1) {
                uint32_t item_vnum;

                if (lua_isnumber(lua, 1)) {
                    item_vnum = (uint32_t)lua_tonumber(lua, 1);
                } else if (lua_isstring(lua, 1)) {
                    if (!get_item_manager().vnum(lua_tostring(lua, 1), item_vnum)) {
                        quest::error("Unknown item name {}", lua_tostring(lua, 1));
                        return 0;
                    }
                } else {
                    quest::error("Invalid item argument");
                    return 0;
                }

                metin2::log::info("Quest: remove item (vnum {:d}) from player {} (pid {:d})", item_vnum, get_quest_manager().current_character_ptr()->name(), get_quest_manager().current_character_ptr()->player_id());
                get_quest_manager().current_character_ptr()->remove_specify_item(item_vnum);
            } else if (lua_gettop(lua) == 2) {
                uint32_t item_vnum;

                if (lua_isnumber(lua, 1)) {
                    item_vnum = (uint32_t)lua_tonumber(lua, 1);
                } else if (lua_isstring(lua, 1)) {
                    if (!get_item_manager().vnum(lua_tostring(lua, 1), item_vnum)) {
                        quest::error("Unknown item name {}", lua_tostring(lua, 1));
                        return 0;
                    }
                } else {
                    quest::error("Invalid item argument");
                    return 0;
                }

                auto item_count = (uint32_t)lua_tonumber(lua, 2);
                metin2::log::info("Quest: remove items (vnum {:d}, count {:d}) from player {} (pid {:d})",
                    item_vnum,
                    item_count,
                    get_quest_manager().current_character_ptr()->name(),
                    get_quest_manager().current_character_ptr()->player_id());

                get_quest_manager().current_character_ptr()->remove_specify_item(item_vnum, item_count);
            }
            return 0;
        }

        auto pc_get_leadership(lua_State *lua) -> int {
            lua_pushnumber(lua, get_quest_manager().current_character_ptr()->leadership_skill_level());
            return 1;
        }

        auto pc_reset_point(lua_State *lua) -> int {
            get_quest_manager().current_character_ptr()->reset_point(get_quest_manager().current_character_ptr()->level());
            return 0;
        }

        auto pc_get_playtime(lua_State *lua) -> int {
            lua_pushnumber(lua, get_quest_manager().current_character_ptr()->real_point(POINT_PLAYTIME));
            return 1;
        }

        auto pc_get_vid(lua_State *lua) -> int {
            lua_pushnumber(lua, get_quest_manager().current_character_ptr()->get_vid());
            return 1;
        }
        auto pc_get_name(lua_State *lua) -> int {
            lua_pushstring(lua, get_quest_manager().current_character_ptr()->name());
            return 1;
        }

        auto pc_get_next_exp(lua_State *lua) -> int {
            lua_pushnumber(lua, get_quest_manager().current_character_ptr()->next_exp());
            return 1;
        }

        auto pc_get_exp(lua_State *lua) -> int {
            lua_pushnumber(lua, get_quest_manager().current_character_ptr()->get_exp());
            return 1;
        }

        auto pc_get_race(lua_State *lua) -> int {
            lua_pushnumber(lua, get_quest_manager().current_character_ptr()->race_num());
            return 1;
        }

        auto pc_change_sex(lua_State *lua) -> int {
            lua_pushnumber(lua, get_quest_manager().current_character_ptr()->change_sex());
            return 1;
        }

        auto pc_get_job(lua_State *lua) -> int {
            lua_pushnumber(lua, get_quest_manager().current_character_ptr()->job());
            return 1;
        }

        auto pc_get_max_sp(lua_State *lua) -> int {
            lua_pushnumber(lua, get_quest_manager().current_character_ptr()->max_sp());
            return 1;
        }

        auto pc_get_sp(lua_State *lua) -> int {
            lua_pushnumber(lua, get_quest_manager().current_character_ptr()->sp());
            return 1;
        }

        auto pc_change_sp(lua_State *lua) -> int {
            if (!require_number(lua, 1)) {
                lua_pushboolean(lua, 0);
                return 1;
            }

            auto val = (int32_t)lua_tonumber(lua, 1);

            if (val == 0) {
                lua_pushboolean(lua, 0);
                return 1;
            }

            character *ch = get_quest_manager().current_character_ptr();

            if (val > 0)
                ch->point_change(POINT_SP, val);
            else if (val < 0) {
                if (ch->sp() < -val) {
                    lua_pushboolean(lua, 0);
                    return 1;
                }

                ch->point_change(POINT_SP, val);
            }

            lua_pushboolean(lua, 1);
            return 1;
        }

        auto pc_get_max_hp(lua_State *lua) -> int {
            lua_pushnumber(lua, get_quest_manager().current_character_ptr()->max_hp());
            return 1;
        }

        auto pc_get_hp(lua_State *lua) -> int {
            lua_pushnumber(lua, get_quest_manager().current_character_ptr()->hp());
            return 1;
        }

        auto pc_get_level(lua_State *lua) -> int {
            lua_pushnumber(lua, get_quest_manager().current_character_ptr()->level());
            return 1;
        }

        auto pc_set_level(lua_State *lua) -> int {
            if (!lua_isnumber(lua, 1)) {
                quest::error("Invalid arguments");
                return 0;
            } else {
                int new_level = lua_tonumber(lua, 1);
                character *ch = get_quest_manager().current_character_ptr();

                metin2::log::info("Quest: player {} jumps to level {:d}", ch->name(), (int)rint(lua_tonumber(lua, 1)));

                pc *pc = get_quest_manager().current_pc();
                get_log_manager().quest_reward_log(pc->current_quest_name().c_str(), ch->player_id(), ch->level(), new_level, 0);

                ch->point_change(POINT_SKILL, new_level - ch->level());
                ch->point_change(POINT_SUB_SKILL, new_level < 10 ? 0 : new_level - std::max(ch->level(), 9));
                ch->point_change(POINT_STAT, ((std::clamp(new_level, 1, 90) - ch->level()) * 3) + ch->point(POINT_LEVEL_STEP));
                ch->point_change(POINT_LEVEL, new_level - ch->level());
                ch->set_random_hp((new_level - 1) * number(job_initial_points[ch->job()].hp_per_lv_begin, job_initial_points[ch->job()].hp_per_lv_end));
                ch->set_random_sp((new_level - 1) * number(job_initial_points[ch->job()].sp_per_lv_begin, job_initial_points[ch->job()].sp_per_lv_end));

                ch->point_change(POINT_HP, ch->max_hp() - ch->hp());
                ch->point_change(POINT_SP, ch->max_sp() - ch->sp());

                ch->compute_points();
                ch->points_packet();
                ch->skill_level_packet();

                return 0;
            }
        }

        auto pc_get_weapon(lua_State *lua) -> int {
            item *item = get_quest_manager().current_character_ptr()->wear(WEAR_WEAPON);

            if (!item)
                lua_pushnumber(lua, 0);
            else
                lua_pushnumber(lua, item->vnum());

            return 1;
        }

        auto pc_get_armor(lua_State *lua) -> int {
            item *item = get_quest_manager().current_character_ptr()->wear(WEAR_BODY);

            if (!item)
                lua_pushnumber(lua, 0);
            else
                lua_pushnumber(lua, item->vnum());

            return 1;
        }

        auto pc_get_wear(lua_State *lua) -> int {
            if (!require_number(lua, 1, "Invalid wear position argument"))
                return 0;

            auto cell = (uint8_t)lua_tonumber(lua, 1);

            item *item = get_quest_manager().current_character_ptr()->wear(cell);

            if (!item)
                lua_pushnil(lua);
            else
                lua_pushnumber(lua, item->vnum());

            return 1;
        }

        auto pc_get_money(lua_State *lua) -> int {
            lua_pushnumber(lua, get_quest_manager().current_character_ptr()->gold());
            return 1;
        }

        auto pc_get_real_alignment(lua_State *lua) -> int {
            lua_pushnumber(lua, get_quest_manager().current_character_ptr()->real_alignment() / 10);
            return 1;
        }

        auto pc_get_alignment(lua_State *lua) -> int {
            lua_pushnumber(lua, get_quest_manager().current_character_ptr()->alignment() / 10);
            return 1;
        }

        auto pc_change_alignment(lua_State *lua) -> int {
            int alignment = (int)(lua_tonumber(lua, 1) * 10);
            character *ch = get_quest_manager().current_character_ptr();

            ch->update_alignment(alignment);
            return 0;
        }

        auto pc_change_money(lua_State *lua) -> int {
            int gold = (int)lua_tonumber(lua, -1);

            character *ch = get_quest_manager().current_character_ptr();

            if (gold + ch->gold() < 0)
                quest::error("Gold change {:d} would make gold negative (now {:d})", gold, ch->gold());
            else {
                get_db_manager().send_money_log(MONEY_LOG_QUEST, ch->player_id(), gold);
                ch->point_change(POINT_GOLD, gold, true);
            }

            return 0;
        }

        auto pc_set_another_quest_flag(lua_State *lua) -> int {
            if (!lua_isstring(lua, 1) || !lua_isstring(lua, 2) || !lua_isnumber(lua, 3)) {
                quest::error("Invalid flag arguments");
                return 0;
            } else {
                const char *sz = lua_tostring(lua, 1);
                const char *sz2 = lua_tostring(lua, 2);
                quest_manager &q = get_quest_manager();
                pc *pc = q.current_pc();
                pc->set_flag(std::string(sz) + "." + sz2, int(rint(lua_tonumber(lua, 3))));
                return 0;
            }
        }

        auto pc_get_another_quest_flag(lua_State *lua) -> int {
            if (!lua_isstring(lua, 1) || !lua_isstring(lua, 2)) {
                quest::error("Invalid flag arguments");
                return 0;
            } else {
                const char *sz = lua_tostring(lua, 1);
                const char *sz2 = lua_tostring(lua, 2);
                quest_manager &q = get_quest_manager();
                pc *pc = q.current_pc();
                if (!pc) {
                    return 0;
                }
                lua_pushnumber(lua, pc->flag(std::string(sz) + "." + sz2));
                return 1;
            }
        }

        auto pc_get_quest_flag(lua_State *lua) -> int {
            if (!lua_isstring(lua, -1)) {
                quest::error("Invalid flag arguments");
                return 0;
            } else {
                const char *sz = lua_tostring(lua, -1);
                quest_manager &q = get_quest_manager();
                pc *pc = q.current_pc();
                lua_pushnumber(lua, pc->flag(pc->current_quest_name() + "." + sz));
                if (metin2::config::test_server())
                    metin2::log::debug("Quest: get flag {}.{}", pc->current_quest_name(), sz);
            }
            return 1;
        }

        auto pc_set_quest_flag(lua_State *lua) -> int {
            if (!lua_isstring(lua, 1) || !lua_isnumber(lua, 2)) {
                quest::error("Invalid flag arguments");
            } else {
                const char *sz = lua_tostring(lua, 1);
                quest_manager &q = get_quest_manager();
                pc *pc = q.current_pc();
                pc->set_flag(pc->current_quest_name() + "." + sz, int(rint(lua_tonumber(lua, 2))));
            }
            return 0;
        }

        auto pc_del_quest_flag(lua_State *lua) -> int {
            if (!require_string(lua, 1, "Invalid flag argument"))
                return 0;

            const char *sz = lua_tostring(lua, 1);
            pc *pc = get_quest_manager().current_pc();
            pc->delete_flag(pc->current_quest_name() + "." + sz);
            return 0;
        }

        auto pc_give_exp2(lua_State *lua) -> int {
            quest_manager &q = get_quest_manager();
            character *ch = q.current_character_ptr();
            if (!lua_isnumber(lua, 1))
                return 0;

            metin2::log::info("Quest reward: player {} gets {:d} exp", ch->name(), (int)rint(lua_tonumber(lua, 1)));

            auto exp = (uint32_t)rint(lua_tonumber(lua, 1));

            pc *pc = get_quest_manager().current_pc();
            get_log_manager().quest_reward_log(pc->current_quest_name().c_str(), ch->player_id(), ch->level(), exp, 0);
            ch->point_change(POINT_EXP, exp);
            return 0;
        }

        auto pc_give_exp(lua_State *lua) -> int {
            if (!lua_isstring(lua, 1) || !lua_isnumber(lua, 2))
                return 0;

            quest_manager &q = get_quest_manager();
            character *ch = q.current_character_ptr();

            metin2::log::info("Quest reward: player {} gets exp ({}) {:d}", ch->name(), lua_tostring(lua, 1), (int)rint(lua_tonumber(lua, 2)));

            auto exp = (uint32_t)rint(lua_tonumber(lua, 2));

            pc *pc = get_quest_manager().current_pc();

            get_log_manager().quest_reward_log(pc->current_quest_name().c_str(), ch->player_id(), ch->level(), exp, 0);

            pc->give_exp(lua_tostring(lua, 1), exp);
            return 0;
        }

        auto pc_give_exp_perc(lua_State *lua) -> int {
            quest_manager &q = get_quest_manager();
            character *ch = q.current_character_ptr();

            if (!ch || !lua_isstring(lua, 1) || !lua_isnumber(lua, 2) || !lua_isnumber(lua, 3))
                return 0;

            int lev = (int)rint(lua_tonumber(lua, 2));
            double proc = (lua_tonumber(lua, 3));

            metin2::log::info("Quest reward: player {} gets exp ({}) level {:d}, {:g}%", ch->name(), lua_tostring(lua, 1), lev, proc);

            uint32_t exp = (uint32_t)((exp_table[std::clamp(lev, 0, PLAYER_EXP_TABLE_MAX)] * proc) / 100);
            pc *pc = get_quest_manager().current_pc();

            get_log_manager().quest_reward_log(pc->current_quest_name().c_str(), ch->player_id(), ch->level(), exp, 0);

            pc->give_exp(lua_tostring(lua, 1), exp);
            return 0;
        }

        auto pc_get_empire(lua_State *lua) -> int {
            lua_pushnumber(lua, get_quest_manager().current_character_ptr()->empire());
            return 1;
        }

        auto pc_get_part(lua_State *lua) -> int {
            quest_manager &q = get_quest_manager();
            character *ch = q.current_character_ptr();
            if (!lua_isnumber(lua, 1)) {
                lua_pushnumber(lua, 0);
                return 1;
            }
            int part_idx = (int)lua_tonumber(lua, 1);
            lua_pushnumber(lua, ch->part(part_idx));
            return 1;
        }

        auto pc_set_part(lua_State *lua) -> int {
            quest_manager &q = get_quest_manager();
            character *ch = q.current_character_ptr();
            if (!lua_isnumber(lua, 1) || !lua_isnumber(lua, 2)) {
                return 0;
            }
            int part_idx = (int)lua_tonumber(lua, 1);
            int part_value = (int)lua_tonumber(lua, 2);
            ch->set_part(part_idx, part_value);
            ch->update_packet();
            return 0;
        }

        auto pc_get_skillgroup(lua_State *lua) -> int {
            lua_pushnumber(lua, get_quest_manager().current_character_ptr()->skill_group());
            return 1;
        }

        auto pc_set_skillgroup(lua_State *lua) -> int {
            if (!lua_isnumber(lua, 1))
                quest::error("Invalid skill group argument");
            else {
                quest_manager &q = get_quest_manager();
                character *ch = q.current_character_ptr();

                ch->set_skill_group((uint8_t)rint(lua_tonumber(lua, 1)));
            }
            return 0;
        }

        auto pc_is_polymorphed(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            lua_pushboolean(lua, ch->is_polymorphed());
            return 1;
        }

        auto pc_remove_polymorph(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            ch->remove_affect(AFFECT_POLYMORPH);
            ch->set_polymorph(0);
            return 0;
        }

        auto pc_polymorph(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            auto vnum = (uint32_t)lua_tonumber(lua, 1);
            int duration = (int)lua_tonumber(lua, 2);
            ch->add_affect(AFFECT_POLYMORPH, POINT_POLYMORPH, vnum, AFF_POLYMORPH, duration, 0, true);
            return 0;
        }

        auto pc_is_mount(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            lua_pushboolean(lua, ch->get_mount_vnum());
            return 1;
        }

        auto pc_mount(lua_State *lua) -> int {
            if (!lua_isnumber(lua, 1))
                return 0;

            int length = 60;

            if (lua_isnumber(lua, 2))
                length = (int)lua_tonumber(lua, 2);

            auto mount_vnum = (uint32_t)lua_tonumber(lua, 1);

            if (length < 0)
                length = 60;

            character *ch = get_quest_manager().current_character_ptr();

            ch->remove_affect(AFFECT_MOUNT);
            ch->remove_affect(AFFECT_MOUNT_BONUS);

            if (ch->horse())
                ch->horse_summon(false);

            if (mount_vnum) {
                ch->add_affect(AFFECT_MOUNT, POINT_MOUNT, mount_vnum, AFF_NONE, length, 0, true);
                switch (mount_vnum) {
                case 20201:
                case 20202:
                case 20203:
                case 20204:
                case 20213:
                case 20216:
                    ch->add_affect(AFFECT_MOUNT, POINT_MOV_SPEED, 30, AFF_NONE, length, 0, true, true);
                    break;

                case 20205:
                case 20206:
                case 20207:
                case 20208:
                case 20214:
                case 20217:
                    ch->add_affect(AFFECT_MOUNT, POINT_MOV_SPEED, 40, AFF_NONE, length, 0, true, true);
                    break;

                case 20209:
                case 20210:
                case 20211:
                case 20212:
                case 20215:
                case 20218:
                    ch->add_affect(AFFECT_MOUNT, POINT_MOV_SPEED, 50, AFF_NONE, length, 0, true, true);
                    break;
                }
            }

            return 0;
        }

        auto pc_mount_bonus(lua_State *lua) -> int {
            auto apply_on = static_cast<uint8_t>(lua_tonumber(lua, 1));
            auto value = static_cast<int32_t>(lua_tonumber(lua, 2));
            auto duration = static_cast<int32_t>(lua_tonumber(lua, 3));

            character *ch = get_quest_manager().current_character_ptr();

            if (nullptr != ch) {
                ch->remove_affect(AFFECT_MOUNT_BONUS);
                ch->add_affect(AFFECT_MOUNT_BONUS, a_apply_info[apply_on].point_type, value, AFF_NONE, duration, 0, false);
            }

            return 0;
        }

        auto pc_unmount(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            ch->remove_affect(AFFECT_MOUNT);
            ch->remove_affect(AFFECT_MOUNT_BONUS);
            if (ch->is_horse_riding())
                ch->stop_riding();
            return 0;
        }

        auto pc_get_horse_level(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            lua_pushnumber(lua, ch->horse_level());
            return 1;
        }

        auto pc_is_horse_alive(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            lua_pushboolean(lua, ch->horse_level() > 0 && ch->horse_health() > 0);
            return 1;
        }

        auto pc_revive_horse(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            ch->revive_horse();
            return 0;
        }

        auto pc_have_map_scroll(lua_State *lua) -> int {
            if (!lua_isstring(lua, 1)) {
                lua_pushboolean(lua, 0);
                return 1;
            }

            const char *map_name = lua_tostring(lua, 1);
            const map_region *region = get_sectree_manager().find_region_by_partial_name(map_name);

            if (!region) {
                lua_pushboolean(lua, 0);
                return 1;
            }

            character *ch = get_quest_manager().current_character_ptr();

            bool find = false;
            for (int cell = 0; cell < INVENTORY_MAX_NUM; cell++) {
                item *item = ch->inventory_item(cell);
                if (!item)
                    continue;

                if (item->type() == ITEM_USE &&
                    item->sub_type() == USE_TALISMAN &&
                    (item->value(0) == 1 || item->value(0) == 2)) {
                    int x = item->get_socket(0);
                    int y = item->get_socket(1);
                    if (region->sx <= x && region->sy <= y && x <= region->ex && y <= region->ey) {
                        find = true;
                        break;
                    }
                }
            }

            lua_pushboolean(lua, find);
            return 1;
        }

        auto pc_get_war_map(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            lua_pushnumber(lua, ch->get_war_map() ? ch->get_war_map()->map_index() : 0);
            return 1;
        }

        auto pc_have_pos_scroll(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();

            if (!lua_isnumber(lua, 1) || !lua_isnumber(lua, 2)) {
                quest::error("Invalid x, y arguments");
                lua_pushboolean(lua, 0);
                return 1;
            }

            if (!require_number(lua, 2, "Invalid radius argument")) {
                lua_pushboolean(lua, 0);
                return 1;
            }

            int x = (int)lua_tonumber(lua, 1);
            int y = (int)lua_tonumber(lua, 2);
            auto r = (float)lua_tonumber(lua, 3);

            bool find = false;
            for (int cell = 0; cell < INVENTORY_MAX_NUM; cell++) {
                item *item = ch->inventory_item(cell);
                if (!item)
                    continue;

                if (item->type() == ITEM_USE &&
                    item->sub_type() == USE_TALISMAN &&
                    (item->value(0) == 1 || item->value(0) == 2)) {
                    int item_x = item->get_socket(0);
                    int item_y = item->get_socket(1);
                    if ((x - item_x) * (x - item_x) + (y - item_y) * (y - item_y) < r * r) {
                        find = true;
                        break;
                    }
                }
            }

            lua_pushboolean(lua, find);
            return 1;
        }

        auto pc_get_equip_refine_level(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            int cell = (int)lua_tonumber(lua, 1);
            if (cell < 0 || cell >= WEAR_MAX_NUM) {
                quest::error("Wear position {:d} is out of range", cell);
                lua_pushnumber(lua, 0);
                return 1;
            }

            item *item = ch->wear(cell);
            if (!item) {
                lua_pushnumber(lua, 0);
                return 1;
            }

            lua_pushnumber(lua, item->refine_level());
            return 1;
        }

        auto pc_refine_equip(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            if (!lua_isnumber(lua, 1) || !lua_isnumber(lua, 2)) {
                quest::error("Invalid arguments");
                lua_pushboolean(lua, 0);
                return 1;
            }

            int cell = (int)lua_tonumber(lua, 1);
            int level_limit = (int)lua_tonumber(lua, 2);
            int pct = lua_isnumber(lua, 3) ? (int)lua_tonumber(lua, 3) : 100;

            item *item = ch->wear(cell);
            if (!item) {
                lua_pushboolean(lua, 0);
                return 1;
            }

            if (item->refined_vnum() == 0) {
                lua_pushboolean(lua, 0);
                return 1;
            }

            if (item->refine_level() > level_limit) {
                lua_pushboolean(lua, 0);
                return 1;
            }

            if (pct == 100 || prob(pct)) {
                lua_pushboolean(lua, 1);

                game::items::item *new_item = get_item_manager().create_item(item->refined_vnum(), 1, 0, false);

                if (new_item) {
                    for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
                        if (!item->get_socket(i))
                            break;
                        else
                            new_item->set_socket(i, 1);

                    int set = 0;
                    for (int i = 0; i < ITEM_SOCKET_MAX_NUM; i++) {
                        int32_t socket = item->get_socket(i);
                        if (socket > 2 && socket != 28960) {
                            new_item->set_socket(set++, socket);
                        }
                    }

                    item->copy_attribute_to(new_item);

                    get_item_manager().remove_item(item, "REMOVE (REFINE SUCCESS)");

                    new_item->equip_to(ch, cell);

                    get_item_manager().flush_delayed_save(new_item);

                    get_log_manager().item_log(ch, new_item, "REFINE SUCCESS (QUEST)", new_item->name());
                }
            } else {
                lua_pushboolean(lua, 0);
            }

            return 1;
        }

        auto pc_get_skill_level(lua_State *lua) -> int {
            if (!require_number(lua, 1)) {
                lua_pushnumber(lua, 0);
                return 1;
            }

            character *ch = get_quest_manager().current_character_ptr();

            auto vnum = (uint32_t)lua_tonumber(lua, 1);
            lua_pushnumber(lua, ch->skill_level(vnum));

            return 1;
        }

        auto pc_give_lotto(lua_State *lua) -> int {
            quest_manager &q = get_quest_manager();
            character *ch = q.current_character_ptr();

            metin2::log::debug("Quest: giving lotto ticket to pid {:d}", ch->player_id());

            get_db_manager().func_query([pid = ch->player_id(), round = static_cast<uint32_t>(q.event_flag("lotto_round"))](sql_msg &msg) { give_lotto_ticket_result(msg, pid, 50001, 1, round); },
                sql::format("INSERT INTO lotto_list VALUES(0, 'server', {}, NOW())", ch->player_id()));

            return 0;
        }

        auto pc_aggregate_monster(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            ch->aggregate_monster();
            return 0;
        }

        auto pc_forget_my_attacker(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            ch->forget_my_attacker();
            return 0;
        }

        auto pc_attract_ranger(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            ch->attract_ranger();
            return 0;
        }

        auto pc_select_vid(lua_State *lua) -> int {
            auto vid = (uint32_t)lua_tonumber(lua, 1);

            character *ch = get_quest_manager().current_character_ptr();
            character *new_ch = get_character_manager().find(vid);

            if (new_ch) {
                get_quest_manager().get_pc(new_ch->player_id());

                lua_pushnumber(lua, (uint32_t)ch->get_vid());
            } else {
                lua_pushnumber(lua, 0);
            }

            return 1;
        }

        auto pc_get_sex(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            lua_pushnumber(lua, get_sex(ch));
            return 1;
        }

        auto pc_is_engaged(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            lua_pushboolean(lua, marriage::get_manager().is_engaged(ch->player_id()));
            return 1;
        }

        auto pc_is_married(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            lua_pushboolean(lua, marriage::get_manager().is_married(ch->player_id()));
            return 1;
        }

        auto pc_is_engaged_or_married(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            lua_pushboolean(lua, marriage::get_manager().is_engaged_or_married(ch->player_id()));
            return 1;
        }

        auto pc_is_gm(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            lua_pushboolean(lua, ch->gm_level() >= GM_HIGH_WIZARD);
            return 1;
        }

        auto pc_get_gm_level(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            lua_pushnumber(lua, ch->gm_level());
            return 1;
        }

        auto pc_mining(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            character *npc = get_quest_manager().current_npc_character_ptr();
            ch->mining(npc);
            return 0;
        }

        auto pc_diamond_refine(lua_State *lua) -> int {
            if (!lua_isnumber(lua, 1)) {
                lua_pushboolean(lua, 0);
                return 1;
            }

            int cost = (int)lua_tonumber(lua, 1);
            int pct = (int)lua_tonumber(lua, 2);

            character *ch = get_quest_manager().current_character_ptr();
            character *npc = get_quest_manager().current_npc_character_ptr();
            item *item = get_quest_manager().current_item();

            if (item)
                lua_pushboolean(lua, mining::ore_refine(ch, npc, item, cost, pct, nullptr));
            else
                lua_pushboolean(lua, 0);

            return 1;
        }

        auto pc_ore_refine(lua_State *lua) -> int {
            if (!lua_isnumber(lua, 1)) {
                lua_pushboolean(lua, 0);
                return 1;
            }

            int cost = (int)lua_tonumber(lua, 1);
            int pct = (int)lua_tonumber(lua, 2);
            int metinstone_cell = (int)lua_tonumber(lua, 3);

            character *ch = get_quest_manager().current_character_ptr();
            character *npc = get_quest_manager().current_npc_character_ptr();
            item *item = get_quest_manager().current_item();

            game::items::item *metinstone_item = ch->inventory_item(metinstone_cell);

            if (item && metinstone_item)
                lua_pushboolean(lua, mining::ore_refine(ch, npc, item, cost, pct, metinstone_item));
            else
                lua_pushboolean(lua, 0);

            return 1;
        }

        auto pc_clear_skill(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            if (ch == nullptr) return 0;

            ch->clear_skill();

            return 0;
        }

        auto pc_clear_sub_skill(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            if (ch == nullptr) return 0;

            ch->clear_sub_skill();

            return 0;
        }

        auto pc_set_skill_point(lua_State *lua) -> int {
            if (!lua_isnumber(lua, 1)) {
                return 0;
            }

            character *ch = get_quest_manager().current_character_ptr();
            int new_point = (int)lua_tonumber(lua, 1);

            ch->set_real_point(POINT_SKILL, new_point);
            ch->set_point(POINT_SKILL, ch->real_point(POINT_SKILL));
            ch->point_change(POINT_SKILL, 0);
            ch->compute_points();
            ch->points_packet();

            return 0;
        }

        auto pc_clear_one_skill(lua_State *lua) -> int {
            int vnum = (int)lua_tonumber(lua, 1);
            metin2::log::debug("Quest: clearing skill {:d}", vnum);

            character *ch = get_quest_manager().current_character_ptr();
            if (ch == nullptr) {
                metin2::log::error("Cannot clear skill {:d}: no current player", vnum);
                lua_pushnumber(lua, 0);
                return 1;
            }

            ch->reset_one_skill(vnum);

            return 0;
        }

        auto pc_is_clear_skill_group(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();

            lua_pushboolean(lua, ch->quest_flag("skill_group_clear.clear") == 1);

            return 1;
        }

        auto pc_save_exit_location(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();

            ch->save_exit_location();

            return 0;
        }

        auto pc_teleport(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            int x = 0, y = 0;
            if (lua_isnumber(lua, 1)) {
                const int town_num = 10;
                struct warp_by_town_name {
                    const char *name;
                    uint32_t x;
                    uint32_t y;
                } ws[town_num] =
                    {
                        {"영안읍성", 4743, 9548},
                        {"임지곡",   3235, 9086},
                        {"자양현",   3531, 8829},
                        {"조안읍성", 638,  1664},
                        {"승룡곡",   1745, 1909},
                        {"복정현",   1455, 2400},
                        {"평무읍성", 9599, 2692},
                        {"방산곡",   8036, 2984},
                        {"박라현",   8639, 2460},
                        {"서한산",   4350, 2143},
                };
                int idx = (int)lua_tonumber(lua, 1);

                x = ws[idx].x;
                y = ws[idx].y;
                goto teleport_area;
            }

            else {
                const char *arg1 = lua_tostring(lua, 1);

                character *tch = get_character_manager().find_pc(arg1);

                if (!tch) {
                    const cci *cci = get_p2p_manager().find(arg1);

                    if (cci) {
                        if (cci->channel != metin2::config::server().channel) {
                            ch->chat_packet(CHAT_TYPE_INFO, "Target is in %d channel (my channel %d)", cci->channel, metin2::config::server().channel);
                        } else {

                            pixel_position pos;

                            if (!get_sectree_manager().center_position_of_map(cci->map_index, pos)) {
                                ch->chat_packet(CHAT_TYPE_INFO, "Cannot find map (index %d)", cci->map_index);
                            } else {
                                ch->chat_packet(CHAT_TYPE_INFO, "You warp to ( %d, %d )", pos.x, pos.y);
                                ch->warp_set(pos.x, pos.y);
                                lua_pushnumber(lua, 1);
                            }
                        }
                    } else if (nullptr == get_character_manager().find_pc(arg1)) {
                        ch->chat_packet(CHAT_TYPE_INFO, "There is no one by that name");
                    }

                    lua_pushnumber(lua, 0);

                    return 1;
                } else {
                    x = tch->x() / 100;
                    y = tch->y() / 100;
                }
            }

        teleport_area:

            x *= 100;
            y *= 100;

            ch->chat_packet(CHAT_TYPE_INFO, "You warp to ( %d, %d )", x, y);
            ch->warp_set(x, y);
            ch->stop();
            lua_pushnumber(lua, 1);
            return 1;
        }

        auto pc_set_skill_level(lua_State *lua) -> int {
            auto vnum = (uint32_t)lua_tonumber(lua, 1);
            auto lev = (uint8_t)lua_tonumber(lua, 2);

            character *ch = get_quest_manager().current_character_ptr();
            ch->set_skill_level(vnum, lev);

            ch->skill_level_packet();

            return 0;
        }

        auto pc_give_polymorph_book(lua_State *lua) -> int {
            if (lua_isnumber(lua, 1) != true && lua_isnumber(lua, 2) != true && lua_isnumber(lua, 3) != true && lua_isnumber(lua, 4) != true) {
                quest::error("Invalid arguments");
                return 0;
            }

            character *ch = get_quest_manager().current_character_ptr();

            get_polymorph_utils().give_book(ch, (uint32_t)lua_tonumber(lua, 1), (uint32_t)lua_tonumber(lua, 2), (uint8_t)lua_tonumber(lua, 3), (uint8_t)lua_tonumber(lua, 4));

            return 0;
        }

        auto pc_upgrade_polymorph_book(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            item *item = get_quest_manager().current_item();

            bool ret = get_polymorph_utils().book_upgrade(ch, item);

            lua_pushboolean(lua, ret);

            return 1;
        }

        auto pc_get_premium_remain_sec(lua_State *lua) -> int {
            int remain_seconds = 0;
            int premium_type = 0;
            character *ch = get_quest_manager().current_character_ptr();

            if (!require_number(lua, 1, "Premium index is not a number"))
                return 0;

            premium_type = (int)lua_tonumber(lua, 1);
            switch (premium_type) {
            case PREMIUM_EXP:
            case PREMIUM_ITEM:
            case PREMIUM_SAFEBOX:
            case PREMIUM_AUTOLOOT:
            case PREMIUM_FISH_MIND:
            case PREMIUM_MARRIAGE_FAST:
            case PREMIUM_GOLD:
                break;

            default:
                quest::error("Unknown premium index {:d}", premium_type);
                return 0;
            }

            remain_seconds = ch->premium_remain_seconds(premium_type);

            lua_pushnumber(lua, remain_seconds);
            return 1;
        }

        auto pc_send_block_mode(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();

            ch->set_block_mode_force((uint8_t)lua_tonumber(lua, 1));

            return 0;
        }

        auto pc_change_empire(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();

            lua_pushnumber(lua, ch->change_empire((unsigned char)lua_tonumber(lua, 1)));

            return 1;
        }

        auto pc_get_change_empire_count(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();

            lua_pushnumber(lua, ch->change_empire_count());

            return 1;
        }

        auto pc_set_change_empire_count(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();

            ch->set_change_empire_count();

            return 0;
        }

        auto pc_change_name(lua_State *lua) -> int {

            character *ch = get_quest_manager().current_character_ptr();

            if (ch->new_name().size() != 0) {
                lua_pushnumber(lua, 0);
                return 1;
            }

            if (lua_isstring(lua, 1) != true) {
                lua_pushnumber(lua, 1);
                return 1;
            }

            const char *name_value = lua_tostring(lua, 1);

            if (check_name(name_value) == false) {
                lua_pushnumber(lua, 2);
                return 1;
            }

            auto pmsg = get_db_manager().direct_query(sql::format("SELECT COUNT(*) FROM player WHERE name={}", name_value));

            if (pmsg->get()->num_rows > 0) {
                MYSQL_ROW row = mysql_fetch_row(pmsg->get()->res.get());

                int count = 0;
                str_to_number(count, row[0]);

                if (count != 0) {
                    lua_pushnumber(lua, 3);
                    return 1;
                }
            }

            uint32_t pid = ch->player_id();
            ch->save();

            get_messenger_manager().remove_all_list(ch->name());

            get_log_manager().change_name_log(pid, ch->name(), name_value, ch->get_desc()->host_name());

            auto msg = get_db_manager().direct_query(sql::format("UPDATE player SET name={} WHERE id={}", name_value, pid));

            ch->set_new_name(name_value);
            lua_pushnumber(lua, 4);
            return 1;
        }

        auto pc_is_dead(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();

            if (ch != nullptr) {
                lua_pushboolean(lua, ch->is_dead());
                return 1;
            }

            lua_pushboolean(lua, true);

            return 1;
        }

        auto pc_reset_status(lua_State *lua) -> int {
            if (lua_isnumber(lua, 1) == true) {
                int idx = (int)lua_tonumber(lua, 1);

                if (idx >= 0 && idx < 4) {
                    character *ch = get_quest_manager().current_character_ptr();
                    int point = POINT_NONE;
                    std::string buf;

                    switch (idx) {
                    case 0:  point = POINT_HT; break;
                    case 1:  point = POINT_IQ; break;
                    case 2:  point = POINT_ST; break;
                    case 3:  point = POINT_DX; break;
                    default: lua_pushboolean(lua, false); return 1;
                    }

                    int old_val = ch->real_point(point);
                    int old_stat = ch->real_point(POINT_STAT);

                    ch->set_real_point(point, 1);
                    ch->set_point(point, ch->real_point(point));

                    ch->point_change(POINT_STAT, old_val - 1);

                    if (point == POINT_HT) {
                        uint8_t job = ch->job();
                        ch->set_random_hp((ch->level() - 1) * number(job_initial_points[job].hp_per_lv_begin, job_initial_points[job].hp_per_lv_end));
                    } else if (point == POINT_IQ) {
                        uint8_t job = ch->job();
                        ch->set_random_sp((ch->level() - 1) * number(job_initial_points[job].sp_per_lv_begin, job_initial_points[job].sp_per_lv_end));
                    }

                    ch->compute_points();
                    ch->points_packet();

                    if (point == POINT_HT) {
                        ch->point_change(POINT_HP, ch->max_hp() - ch->hp());
                    } else if (point == POINT_IQ) {
                        ch->point_change(POINT_SP, ch->max_sp() - ch->sp());
                    }

                    switch (idx) {
                    case 0:
                        buf = std::format("reset ht({})->1 stat_point({})->({})", old_val, old_stat, ch->real_point(POINT_STAT));
                        break;
                    case 1:
                        buf = std::format("reset iq({})->1 stat_point({})->({})", old_val, old_stat, ch->real_point(POINT_STAT));
                        break;
                    case 2:
                        buf = std::format("reset st({})->1 stat_point({})->({})", old_val, old_stat, ch->real_point(POINT_STAT));
                        break;
                    case 3:
                        buf = std::format("reset dx({})->1 stat_point({})->({})", old_val, old_stat, ch->real_point(POINT_STAT));
                        break;
                    }

                    get_log_manager().char_log(ch, 0, "RESET_ONE_STATUS", buf.c_str());

                    lua_pushboolean(lua, true);
                    return 1;
                }
            }

            lua_pushboolean(lua, false);
            return 1;
        }

        auto pc_get_ht(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            lua_pushnumber(lua, ch->real_point(POINT_HT));
            return 1;
        }

        auto pc_set_ht(lua_State *lua) -> int {
            if (lua_isnumber(lua, 1) == false)
                return 1;

            int new_point = (int)lua_tonumber(lua, 1);

            character *ch = get_quest_manager().current_character_ptr();
            int used_point = new_point - ch->real_point(POINT_HT);
            ch->set_real_point(POINT_HT, new_point);
            ch->point_change(POINT_HT, 0);
            ch->point_change(POINT_STAT, -used_point);
            ch->compute_points();
            ch->points_packet();
            return 1;
        }

        auto pc_get_iq(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            lua_pushnumber(lua, ch->real_point(POINT_IQ));
            return 1;
        }

        auto pc_set_iq(lua_State *lua) -> int {
            if (lua_isnumber(lua, 1) == false)
                return 1;

            int new_point = (int)lua_tonumber(lua, 1);

            character *ch = get_quest_manager().current_character_ptr();
            int used_point = new_point - ch->real_point(POINT_IQ);
            ch->set_real_point(POINT_IQ, new_point);
            ch->point_change(POINT_IQ, 0);
            ch->point_change(POINT_STAT, -used_point);
            ch->compute_points();
            ch->points_packet();
            return 1;
        }

        auto pc_get_st(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            lua_pushnumber(lua, ch->real_point(POINT_ST));
            return 1;
        }

        auto pc_set_st(lua_State *lua) -> int {
            if (lua_isnumber(lua, 1) == false)
                return 1;

            int new_point = (int)lua_tonumber(lua, 1);

            character *ch = get_quest_manager().current_character_ptr();
            int used_point = new_point - ch->real_point(POINT_ST);
            ch->set_real_point(POINT_ST, new_point);
            ch->point_change(POINT_ST, 0);
            ch->point_change(POINT_STAT, -used_point);
            ch->compute_points();
            ch->points_packet();
            return 1;
        }

        auto pc_get_dx(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            lua_pushnumber(lua, ch->real_point(POINT_DX));
            return 1;
        }

        auto pc_set_dx(lua_State *lua) -> int {
            if (lua_isnumber(lua, 1) == false)
                return 1;

            int new_point = (int)lua_tonumber(lua, 1);

            character *ch = get_quest_manager().current_character_ptr();
            int used_point = new_point - ch->real_point(POINT_DX);
            ch->set_real_point(POINT_DX, new_point);
            ch->point_change(POINT_DX, 0);
            ch->point_change(POINT_STAT, -used_point);
            ch->compute_points();
            ch->points_packet();
            return 1;
        }

        auto pc_is_near_vid(lua_State *lua) -> int {
            if (lua_isnumber(lua, 1) != true || lua_isnumber(lua, 2) != true) {
                lua_pushboolean(lua, false);
            } else {
                character *me = get_quest_manager().current_character_ptr();
                character *other = get_character_manager().find((uint32_t)lua_tonumber(lua, 1));

                if (me != nullptr && other != nullptr) {
                    lua_pushboolean(lua, (distance_approx(me->x() - other->x(), me->y() - other->y()) < (int)lua_tonumber(lua, 2) * 100));
                } else {
                    lua_pushboolean(lua, false);
                }
            }

            return 1;
        }

        auto pc_get_socket_items(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();

            lua_newtable(lua);

            if (ch == nullptr) return 1;

            int idx = 1;

            for (int i = 0; i < INVENTORY_MAX_NUM + WEAR_MAX_NUM; i++) {
                item *item = ch->inventory_item(i);

                if (item != nullptr) {
                    if (item->is_equipped() == false) {
                        int j = 0;
                        for (; j < ITEM_SOCKET_MAX_NUM; j++) {
                            int32_t socket = item->get_socket(j);

                            if (socket > 2 && socket != ITEM_BROKEN_METIN_VNUM) {
                                item_table *item_info = get_item_manager().table(socket);
                                if (item_info != nullptr) {
                                    if (item_info->type == ITEM_METIN) break;
                                }
                            }
                        }

                        if (j >= ITEM_SOCKET_MAX_NUM) continue;

                        lua_newtable(lua);

                        {
                            lua_pushstring(lua, item->name());
                            lua_rawseti(lua, -2, 1);

                            lua_pushnumber(lua, i);
                            lua_rawseti(lua, -2, 2);
                        }

                        lua_rawseti(lua, -2, idx++);
                    }
                }
            }

            return 1;
        }

        auto pc_get_empty_inventory_count(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();

            if (ch != nullptr) {
                lua_pushnumber(lua, ch->count_empty_inventory());
            } else {
                lua_pushnumber(lua, 0);
            }

            return 1;
        }

        auto pc_get_logoff_interval(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();

            if (ch != nullptr) {
                lua_pushnumber(lua, ch->log_off_interval());
            } else {
                lua_pushnumber(lua, 0);
            }

            return 1;
        }

        auto pc_get_player_id(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();

            if (ch != nullptr) {
                lua_pushnumber(lua, ch->player_id());
            } else {
                lua_pushnumber(lua, 0);
            }

            return 1;
        }

        auto pc_get_account_id(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();

            if (ch != nullptr) {
                if (ch->get_desc() != nullptr) {
                    lua_pushnumber(lua, ch->get_desc()->get_account_table().id);
                    return 1;
                }
            }

            lua_pushnumber(lua, 0);
            return 1;
        }

        auto pc_get_account(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();

            if (nullptr != ch) {
                if (nullptr != ch->get_desc()) {
                    lua_pushstring(lua, ch->get_desc()->get_account_table().login);
                    return 1;
                }
            }

            lua_pushstring(lua, "");
            return 1;
        }

        auto pc_is_riding(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();

            if (nullptr != ch) {
                bool is_riding = ch->is_riding();

                lua_pushboolean(lua, is_riding);

                return 1;
            }

            lua_pushboolean(lua, false);
            return 1;
        }

        auto pc_get_special_ride_vnum(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();

            if (nullptr != ch) {
                item *unique1 = ch->wear(WEAR_UNIQUE1);
                item *unique2 = ch->wear(WEAR_UNIQUE2);

                if (nullptr != unique1) {
                    if (UNIQUE_GROUP_SPECIAL_RIDE == unique1->special_group()) {
                        lua_pushnumber(lua, unique1->vnum());
                        lua_pushnumber(lua, unique1->get_socket(2));
                        return 2;
                    }
                }

                if (nullptr != unique2) {
                    if (UNIQUE_GROUP_SPECIAL_RIDE == unique2->special_group()) {
                        lua_pushnumber(lua, unique2->vnum());
                        lua_pushnumber(lua, unique2->get_socket(2));
                        return 2;
                    }
                }
            }

            lua_pushnumber(lua, 0);
            lua_pushnumber(lua, 0);

            return 2;
        }

        auto pc_can_warp(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();

            if (nullptr != ch) {
                lua_pushboolean(lua, ch->can_warp());
            } else {
                lua_pushboolean(lua, false);
            }

            return 1;
        }

        auto pc_dec_skill_point(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();

            if (nullptr != ch) {
                ch->point_change(POINT_SKILL, -1);
            }

            return 0;
        }

        auto pc_get_skill_point(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();

            if (nullptr != ch) {
                lua_pushnumber(lua, ch->point(POINT_SKILL));
            } else {
                lua_pushnumber(lua, 0);
            }

            return 1;
        }

        auto pc_get_channel_id(lua_State *lua) -> int {
            lua_pushnumber(lua, metin2::config::server().channel);

            return 1;
        }

        auto pc_give_poly_marble(lua_State *lua) -> int {
            const int vnum = lua_tonumber(lua, 1);

            const mob *mob_info = get_mob_manager().get(vnum);

            if (nullptr == mob_info) {
                lua_pushboolean(lua, false);
                return 1;
            }

            if (0 == mob_info->table.polymorph_item_vnum) {
                lua_pushboolean(lua, false);
                return 1;
            }

            item *item = get_item_manager().create_item(mob_info->table.polymorph_item_vnum);

            if (nullptr == item) {
                lua_pushboolean(lua, false);
                return 1;
            }

            item->set_socket(0, vnum);

            character *const ch = get_quest_manager().current_character_ptr();

            int empty_cell = ch->empty_inventory(item->size());

            if (-1 == empty_cell) {
                get_item_manager().destroy_item(item);
                lua_pushboolean(lua, false);
                return 1;
            }

            item->add_to_character(ch, item_pos(INVENTORY, empty_cell));

            const pc *pc = get_quest_manager().current_pc();

            get_log_manager().quest_reward_log(pc->current_quest_name().c_str(), ch->player_id(), ch->level(), mob_info->table.polymorph_item_vnum, vnum);

            lua_pushboolean(lua, true);

            return 1;
        }

        auto pc_get_sig_items(lua_State *lua) -> int {
            auto group_vnum = (uint32_t)lua_tonumber(lua, 1);
            character *const ch = get_quest_manager().current_character_ptr();
            int count = 0;
            for (int i = 0; i < INVENTORY_MAX_NUM; ++i) {
                if (ch->inventory_item(i) != nullptr && ch->inventory_item(i)->sig_vnum() == group_vnum) {
                    lua_pushnumber(lua, ch->inventory_item(i)->id());
                    count++;
                }
            }

            return count;
        }

        auto pc_charge_cash(lua_State *lua) -> int {
            request_charge_cash packet;

            int amount = lua_isnumber(lua, 1) ? (int)lua_tonumber(lua, 1) : 0;
            std::string charge_type = lua_isstring(lua, 2) ? lua_tostring(lua, 2) : "";

            character *ch = get_quest_manager().current_character_ptr();

            if (nullptr == ch || nullptr == ch->get_desc() || 1 > amount || 50000 < amount) {
                lua_pushboolean(lua, 0);
                return 1;
            }

            packet.aid = ch->get_desc()->get_account_table().id;
            packet.amount = (uint32_t)amount;
            packet.charge_type = ERequestCharge_Cash;

            if (0 < charge_type.length())
                std::ranges::transform(charge_type, charge_type.begin(), (int (*)(int))std::tolower);

            if ("mileage" == charge_type)
                packet.charge_type = ERequestCharge_Mileage;

            db_clientdesc->db_packet_header(HEADER_GD_REQUEST_CHARGE_CASH, 0, sizeof(request_charge_cash));
            db_clientdesc->packet(&packet, sizeof(packet));

            lua_pushboolean(lua, 1);
            return 1;
        }

        auto pc_give_award(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();

            if (!lua_isnumber(lua, 1) || !lua_isnumber(lua, 2) || !lua_isstring(lua, 3)) {
                quest::error("Invalid award arguments");
                lua_pushnumber(lua, 0);
                return 1;
            }

            uint32_t vnum_value = (int)lua_tonumber(lua, 1);

            int icount = (int)lua_tonumber(lua, 2);

            metin2::log::info("Quest award: item (vnum {:d}) to account {}", vnum_value, ch->get_desc()->get_account_table().login);

            get_db_manager().query(sql::format("INSERT INTO item_award (login, vnum, count, given_time, why, mall)select {}, {}, {}, now(), {}, 1 from DUAL where not exists (select login, why from item_award where login = {} and why  = {}) ;",
                ch->get_desc()->get_account_table().login,
                vnum_value,
                icount,
                lua_tostring(lua, 3),
                ch->get_desc()->get_account_table().login,
                lua_tostring(lua, 3)));

            lua_pushnumber(lua, 0);
            return 1;
        }
        auto pc_give_award_socket(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();

            if (!lua_isnumber(lua, 1) || !lua_isnumber(lua, 2) || !lua_isstring(lua, 3) || !lua_isstring(lua, 4) || !lua_isstring(lua, 5) || !lua_isstring(lua, 6)) {
                quest::error("Invalid award arguments");
                lua_pushnumber(lua, 0);
                return 1;
            }

            uint32_t vnum_value = (int)lua_tonumber(lua, 1);

            int icount = (int)lua_tonumber(lua, 2);

            metin2::log::info("Quest award: item (vnum {:d}) to account {}", vnum_value, ch->get_desc()->get_account_table().login);

            int64_t sockets[3] = {};
            for (int i = 0; i < 3; ++i)
                str_to_number(sockets[i], lua_tostring(lua, 4 + i));

            get_db_manager().query(sql::format("INSERT INTO item_award (login, vnum, count, given_time, why, mall, socket0, socket1, socket2)select {}, {}, {}, now(), {}, 1, {}, {}, {} from DUAL where not exists (select login, why from item_award where login = {} and why  = {}) ;",
                ch->get_desc()->get_account_table().login,
                vnum_value,
                icount,
                lua_tostring(lua, 3),
                sockets[0],
                sockets[1],
                sockets[2],
                ch->get_desc()->get_account_table().login,
                lua_tostring(lua, 3)));

            lua_pushnumber(lua, 0);
            return 1;
        }

        auto pc_get_informer_type(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();

            if (ch != nullptr) {
                lua_pushstring(lua, ch->item_award_cmd());
            } else
                lua_pushstring(lua, "");

            return 1;
        }

        auto pc_get_informer_item(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();

            if (ch != nullptr) {
                lua_pushnumber(lua, ch->item_award_vnum());
            } else
                lua_pushnumber(lua, 0);

            return 1;
        }

        auto pc_get_killee_drop_pct(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            character *killee = ch->quest_npc();

            int delta_percent, rand_range;
            if (nullptr == killee || !get_item_manager().drop_pct(killee, ch, delta_percent, rand_range)) {
                quest::error("No killed mob or no drop chance for player {}", ch->name());
                lua_pushnumber(lua, -1);
                lua_pushnumber(lua, -1);

                return 2;
            }

            lua_pushnumber(lua, delta_percent);
            lua_pushnumber(lua, rand_range);

            return 2;
        }

        void register_pc_function_table() {
            luaL_reg pc_functions[] =
                {
                    {"get_wear",                            pc_get_wear                           },
                    {"get_player_id",                       pc_get_player_id                      },
                    {"get_account_id",                      pc_get_account_id                     },
                    {"get_account",                         pc_get_account                        },
                    {"get_level",                           pc_get_level                          },
                    {"set_level",                           pc_set_level                          },
                    {"get_next_exp",                        pc_get_next_exp                       },
                    {"get_exp",                             pc_get_exp                            },
                    {"get_job",                             pc_get_job                            },
                    {"get_race",                            pc_get_race                           },
                    {"change_sex",                          pc_change_sex                         },
                    {"gethp",                               pc_get_hp                             },
                    {"get_hp",                              pc_get_hp                             },
                    {"getmaxhp",                            pc_get_max_hp                         },
                    {"get_max_hp",                          pc_get_max_hp                         },
                    {"getsp",                               pc_get_sp                             },
                    {"get_sp",                              pc_get_sp                             },
                    {"getmaxsp",                            pc_get_max_sp                         },
                    {"get_max_sp",                          pc_get_max_sp                         },
                    {"change_sp",                           pc_change_sp                          },
                    {"getmoney",                            pc_get_money                          },
                    {"get_money",                           pc_get_money                          },
                    {"get_real_alignment",                  pc_get_real_alignment                 },
                    {"get_alignment",                       pc_get_alignment                      },
                    {"getweapon",                           pc_get_weapon                         },
                    {"get_weapon",                          pc_get_weapon                         },
                    {"getarmor",                            pc_get_armor                          },
                    {"get_armor",                           pc_get_armor                          },
                    {"getgold",                             pc_get_money                          },
                    {"get_gold",                            pc_get_money                          },
                    {"changegold",                          pc_change_money                       },
                    {"changemoney",                         pc_change_money                       },
                    {"changealignment",                     pc_change_alignment                   },
                    {"change_gold",                         pc_change_money                       },
                    {"change_money",                        pc_change_money                       },
                    {"change_alignment",                    pc_change_alignment                   },
                    {"getname",                             pc_get_name                           },
                    {"get_name",                            pc_get_name                           },
                    {"get_vid",                             pc_get_vid                            },
                    {"getplaytime",                         pc_get_playtime                       },
                    {"get_playtime",                        pc_get_playtime                       },
                    {"getleadership",                       pc_get_leadership                     },
                    {"get_leadership",                      pc_get_leadership                     },
                    {"getqf",                               pc_get_quest_flag                     },
                    {"setqf",                               pc_set_quest_flag                     },
                    {"delqf",                               pc_del_quest_flag                     },
                    {"getf",                                pc_get_another_quest_flag             },
                    {"setf",                                pc_set_another_quest_flag             },
                    {"get_x",                               pc_get_x                              },
                    {"get_y",                               pc_get_y                              },
                    {"getx",                                pc_get_x                              },
                    {"gety",                                pc_get_y                              },
                    {"get_local_x",                         pc_get_local_x                        },
                    {"get_local_y",                         pc_get_local_y                        },
                    {"getcurrentmapindex",                  pc_get_current_map_index              },
                    {"get_map_index",                       pc_get_current_map_index              },
                    {"give_exp",                            pc_give_exp                           },
                    {"give_exp_perc",                       pc_give_exp_perc                      },
                    {"give_exp2",                           pc_give_exp2                          },
                    {"give_item",                           pc_give_item                          },
                    {"give_item2",                          pc_give_or_drop_item                  },
                    {"give_item2_select",                   pc_give_or_drop_item_and_select       },
                    {"give_gold",                           pc_give_gold                          },
                    {"count_item",                          pc_count_item                         },
                    {"remove_item",                         pc_remove_item                        },
                    {"countitem",                           pc_count_item                         },
                    {"removeitem",                          pc_remove_item                        },
                    {"reset_point",                         pc_reset_point                        },
                    {"has_guild",                           pc_hasguild                           },
                    {"hasguild",                            pc_hasguild                           },
                    {"get_guild",                           pc_getguild                           },
                    {"getguild",                            pc_getguild                           },
                    {"isguildmaster",                       pc_isguildmaster                      },
                    {"is_guild_master",                     pc_isguildmaster                      },
                    {"destroy_guild",                       pc_destroy_guild                      },
                    {"remove_from_guild",                   pc_remove_from_guild                  },
                    {"in_dungeon",                          pc_in_dungeon                         },
                    {"getempire",                           pc_get_empire                         },
                    {"get_empire",                          pc_get_empire                         },
                    {"get_skill_group",                     pc_get_skillgroup                     },
                    {"set_skill_group",                     pc_set_skillgroup                     },
                    {"warp",                                pc_warp                               },
                    {"warp_local",                          pc_warp_local                         },
                    {"warp_exit",                           pc_warp_exit                          },
                    {"set_warp_location",                   pc_set_warp_location                  },
                    {"set_warp_location_local",             pc_set_warp_location_local            },
                    {"get_start_location",                  pc_get_start_location                 },
                    {"has_master_skill",                    pc_has_master_skill                   },
                    {"set_part",                            pc_set_part                           },
                    {"get_part",                            pc_get_part                           },
                    {"is_polymorphed",                      pc_is_polymorphed                     },
                    {"remove_polymorph",                    pc_remove_polymorph                   },
                    {"is_mount",                            pc_is_mount                           },
                    {"polymorph",                           pc_polymorph                          },
                    {"mount",                               pc_mount                              },
                    {"mount_bonus",                         pc_mount_bonus                        },
                    {"unmount",                             pc_unmount                            },
                    {"warp_to_guild_war_observer_position", pc_warp_to_guild_war_observer_position},
                    {"give_item_from_special_item_group",   pc_give_item_from_special_item_group  },
                    {"learn_grand_master_skill",            pc_learn_grand_master_skill           },
                    {"is_skill_book_no_delay",              pc_is_skill_book_no_delay             },
                    {"remove_skill_book_no_delay",          pc_remove_skill_book_no_delay         },

                    {"enough_inventory",                    pc_enough_inventory                   },
                    {"get_horse_level",                     pc_get_horse_level                    },
                    {"is_horse_alive",                      pc_is_horse_alive                     },
                    {"revive_horse",                        pc_revive_horse                       },
                    {"have_pos_scroll",                     pc_have_pos_scroll                    },
                    {"have_map_scroll",                     pc_have_map_scroll                    },
                    {"get_war_map",                         pc_get_war_map                        },
                    {"get_equip_refine_level",              pc_get_equip_refine_level             },
                    {"refine_equip",                        pc_refine_equip                       },
                    {"get_skill_level",                     pc_get_skill_level                    },
                    {"give_lotto",                          pc_give_lotto                         },
                    {"aggregate_monster",                   pc_aggregate_monster                  },
                    {"forget_my_attacker",                  pc_forget_my_attacker                 },
                    {"pc_attract_ranger",                   pc_attract_ranger                     },
                    {"select",                              pc_select_vid                         },
                    {"get_sex",                             pc_get_sex                            },
                    {"is_married",                          pc_is_married                         },
                    {"is_engaged",                          pc_is_engaged                         },
                    {"is_engaged_or_married",               pc_is_engaged_or_married              },
                    {"is_gm",                               pc_is_gm                              },
                    {"get_gm_level",                        pc_get_gm_level                       },
                    {"mining",                              pc_mining                             },
                    {"ore_refine",                          pc_ore_refine                         },
                    {"diamond_refine",                      pc_diamond_refine                     },

                    {"clear_one_skill",                     pc_clear_one_skill                    },

                    {"clear_skill",                         pc_clear_skill                        },
                    {"clear_sub_skill",                     pc_clear_sub_skill                    },
                    {"set_skill_point",                     pc_set_skill_point                    },

                    {"is_clear_skill_group",                pc_is_clear_skill_group               },

                    {"save_exit_location",                  pc_save_exit_location                 },
                    {"teleport",                            pc_teleport                           },

                    {"set_skill_level",                     pc_set_skill_level                    },

                    {"give_polymorph_book",                 pc_give_polymorph_book                },
                    {"upgrade_polymorph_book",              pc_upgrade_polymorph_book             },
                    {"get_premium_remain_sec",              pc_get_premium_remain_sec             },

                    {"send_block_mode",                     pc_send_block_mode                    },

                    {"change_empire",                       pc_change_empire                      },
                    {"get_change_empire_count",             pc_get_change_empire_count            },
                    {"set_change_empire_count",             pc_set_change_empire_count            },

                    {"change_name",                         pc_change_name                        },

                    {"is_dead",                             pc_is_dead                            },

                    {"reset_status",                        pc_reset_status                       },
                    {"get_ht",                              pc_get_ht                             },
                    {"set_ht",                              pc_set_ht                             },
                    {"get_iq",                              pc_get_iq                             },
                    {"set_iq",                              pc_set_iq                             },
                    {"get_st",                              pc_get_st                             },
                    {"set_st",                              pc_set_st                             },
                    {"get_dx",                              pc_get_dx                             },
                    {"set_dx",                              pc_set_dx                             },

                    {"is_near_vid",                         pc_is_near_vid                        },

                    {"get_socket_items",                    pc_get_socket_items                   },
                    {"get_empty_inventory_count",           pc_get_empty_inventory_count          },

                    {"get_logoff_interval",                 pc_get_logoff_interval                },

                    {"is_riding",                           pc_is_riding                          },
                    {"get_special_ride_vnum",               pc_get_special_ride_vnum              },

                    {"can_warp",                            pc_can_warp                           },

                    {"dec_skill_point",                     pc_dec_skill_point                    },
                    {"get_skill_point",                     pc_get_skill_point                    },

                    {"get_channel_id",                      pc_get_channel_id                     },

                    {"give_poly_marble",                    pc_give_poly_marble                   },
                    {"get_sig_items",                       pc_get_sig_items                      },

                    {"charge_cash",                         pc_charge_cash                        },

                    {"get_informer_type",                   pc_get_informer_type                  },
                    {"get_informer_item",                   pc_get_informer_item                  },

                    {"give_award",                          pc_give_award                         },
                    {"give_award_socket",                   pc_give_award_socket                  },

                    {"get_killee_drop_pct",                 pc_get_killee_drop_pct                },

                    {nullptr,                               nullptr                               }
            };

            get_quest_manager().add_lua_function_table("pc", pc_functions);
        }
    };
}
