#include <metin2/game/app/typedef.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/parties/party.h>
#include <metin2/game/protocol/buffer_manager.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/quest/lua_incl.h>
#include <metin2/game/quest/questlua.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/world/dungeon.h>
#include <metin2/game/world/sectree_manager.h>

#include <metin2/core/utils.h>
#include <metin2/log/log.h>

#include <cmath>
#include <cstdint>
#include <numbers>
#include <optional>
#include <string>
#include <utility>

namespace metin2::game {

    template <class Func>
    auto dungeon::for_each_member(Func f) -> Func {
        decltype(set_character_.begin()) it;

        for (it = set_character_.begin(); it != set_character_.end(); ++it) {
            metin2::log::debug("Dungeon: visiting member {}", (*it)->name());
            f(*it);
        }
        return f;
    }

    namespace quest {
        namespace {
            auto current_dungeon() -> dungeon * {
                return get_quest_manager().current_dungeon();
            }

            auto current_dungeon_or_error() -> dungeon * {
                dungeon *dungeon_value = current_dungeon();
                if (!dungeon_value)
                    quest::error("Not in a dungeon");
                return dungeon_value;
            }

            auto set_members_quest_flag(const std::string &flagname, int value) {
                if (dungeon *dungeon = current_dungeon())
                    dungeon->for_each_member([&](character *ch) { set_pc_quest_flag(ch, flagname, value); });
            }

            struct item_group_target {
                dungeon *owner;
                sectree_map *map;
                const dungeon::item_group *group;
            };

            auto current_item_group(std::string group_name) -> std::optional<item_group_target> {
                dungeon *dungeon = current_dungeon_or_error();
                if (!dungeon)
                    return std::nullopt;

                sectree_map *map_value = get_sectree_manager().map(dungeon->map_index());
                if (!map_value) {
                    quest::error("No map with index {:d}", dungeon->map_index());
                    return std::nullopt;
                }

                const dungeon::item_group *group = dungeon->get_item_group(group_name);
                if (group == nullptr) {
                    quest::error("Unknown item group {}", group_name);
                    return std::nullopt;
                }
                return item_group_target{dungeon, map_value, group};
            }
        }

        auto dungeon_notice(lua_State *lua) -> int {
            if (!lua_isstring(lua, 1))
                return 0;

            if (dungeon *dungeon = current_dungeon())
                dungeon->notice(lua_tostring(lua, 1));
            return 0;
        }

        auto dungeon_set_quest_flag(lua_State *lua) -> int {
            std::string flagname = get_quest_manager().current_pc()->current_quest_name() + "." + lua_tostring(lua, 1);
            set_members_quest_flag(flagname, (int)rint(lua_tonumber(lua, 2)));
            return 0;
        }

        auto dungeon_set_flag(lua_State *lua) -> int {
            if (!lua_isstring(lua, 1) || !lua_isnumber(lua, 2)) {
                quest::error("Invalid flag arguments");
            } else if (dungeon *dungeon = current_dungeon_or_error()) {
                dungeon->set_flag(lua_tostring(lua, 1), int(lua_tonumber(lua, 2)));
            }
            return 0;
        }

        auto dungeon_get_flag(lua_State *lua) -> int {
            if (!lua_isstring(lua, 1)) {
                quest::error("Invalid flag arguments");
            }

            dungeon *dungeon = current_dungeon_or_error();
            lua_pushnumber(lua, dungeon ? dungeon->flag(lua_tostring(lua, 1)) : 0);
            return 1;
        }

        auto dungeon_get_flag_from_map_index(lua_State *lua) -> int {
            if (!lua_isstring(lua, 1) || !lua_isnumber(lua, 2)) {
                quest::error("Invalid flag arguments");
            }

            auto map_index = (uint32_t)lua_tonumber(lua, 2);
            if (map_index) {
                dungeon *dungeon_value = get_dungeon_manager().find_by_map_index(map_index);
                if (dungeon_value) {
                    const char *sz = lua_tostring(lua, 1);
                    lua_pushnumber(lua, dungeon_value->flag(sz));
                } else {
                    quest::error("Not in a dungeon");
                    lua_pushnumber(lua, 0);
                }
            } else {
                lua_pushboolean(lua, 0);
            }
            return 1;
        }

        auto dungeon_get_map_index(lua_State *lua) -> int {
            dungeon *dungeon = current_dungeon_or_error();
            if (dungeon)
                metin2::log::debug("Dungeon: map index {:d}", dungeon->map_index());
            lua_pushnumber(lua, dungeon ? dungeon->map_index() : 0);
            return 1;
        }

        auto dungeon_regen_file(lua_State *lua) -> int {
            if (!require_string(lua, 1, "Invalid file name argument"))
                return 0;

            if (dungeon *dungeon = current_dungeon())
                dungeon->spawn_regen(lua_tostring(lua, 1));

            return 0;
        }

        auto dungeon_set_regen_file(lua_State *lua) -> int {
            if (!require_string(lua, 1, "Invalid file name argument"))
                return 0;
            if (dungeon *dungeon = current_dungeon())
                dungeon->spawn_regen(lua_tostring(lua, 1), false);
            return 0;
        }

        auto dungeon_clear_regen(lua_State *lua) -> int {
            if (dungeon *dungeon = current_dungeon())
                dungeon->clear_regen();
            return 0;
        }

        auto dungeon_check_eliminated(lua_State *lua) -> int {
            if (dungeon *dungeon = current_dungeon())
                dungeon->check_eliminated();
            return 0;
        }

        auto dungeon_set_exit_all_at_eliminate(lua_State *lua) -> int {
            if (!require_number(lua, 1, "Invalid time argument"))
                return 0;

            if (dungeon *dungeon = current_dungeon())
                dungeon->set_exit_all_at_eliminate((int32_t)lua_tonumber(lua, 1));

            return 0;
        }

        auto dungeon_set_warp_at_eliminate(lua_State *lua) -> int {
            if (!require_number(lua, 1, "Invalid time argument"))
                return 0;

            if (!require_number(lua, 2, "Invalid map index argument"))
                return 0;

            if (!require_number(lua, 3, "Invalid x argument"))
                return 0;

            if (!require_number(lua, 4, "Invalid y argument"))
                return 0;

            const char *c_psz_regen_file = nullptr;

            if (lua_gettop(lua) >= 5)
                c_psz_regen_file = lua_tostring(lua, 5);

            if (dungeon *dungeon = current_dungeon_or_error()) {
                dungeon->set_warp_at_eliminate((int32_t)lua_tonumber(lua, 1),
                    (int32_t)lua_tonumber(lua, 2),
                    (int32_t)lua_tonumber(lua, 3),
                    (int32_t)lua_tonumber(lua, 4),
                    c_psz_regen_file);
            }

            return 0;
        }

        auto dungeon_new_jump(lua_State *lua) -> int {
            if (lua_gettop(lua) < 3) {
                quest::error("Not enough arguments");
                return 0;
            }

            if (!lua_isnumber(lua, 1) || !lua_isnumber(lua, 2) || !lua_isnumber(lua, 3)) {
                quest::error("Invalid arguments");
                return 0;
            }

            auto map_index = (int32_t)lua_tonumber(lua, 1);

            dungeon *dungeon_value = get_dungeon_manager().create(map_index);

            if (!dungeon_value) {
                quest::error("Cannot create dungeon on map {:d}", map_index);
                return 0;
            }

            character *ch = get_quest_manager().current_character_ptr();

            ch->warp_set((int)lua_tonumber(lua, 2), (int)lua_tonumber(lua, 3), dungeon_value->map_index());
            return 0;
        }

        auto dungeon_new_jump_all(lua_State *lua) -> int {
            if (lua_gettop(lua) < 3 || !lua_isnumber(lua, 1) || !lua_isnumber(lua, 2) || !lua_isnumber(lua, 3)) {
                quest::error("Not enough arguments");
                return 0;
            }

            auto map_index = (int32_t)lua_tonumber(lua, 1);

            dungeon *dungeon_value = get_dungeon_manager().create(map_index);

            if (!dungeon_value) {
                quest::error("Cannot create dungeon on map {:d}", map_index);
                return 0;
            }

            character *ch = get_quest_manager().current_character_ptr();

            dungeon_value->jump_all(ch->map_index(), (int)lua_tonumber(lua, 2), (int)lua_tonumber(lua, 3));

            return 0;
        }

        auto dungeon_new_jump_party(lua_State *lua) -> int {
            if (lua_gettop(lua) < 3 || !lua_isnumber(lua, 1) || !lua_isnumber(lua, 2) || !lua_isnumber(lua, 3)) {
                quest::error("Not enough arguments");
                return 0;
            }

            auto map_index = (int32_t)lua_tonumber(lua, 1);

            dungeon *dungeon_value = get_dungeon_manager().create(map_index);

            if (!dungeon_value) {
                quest::error("Cannot create dungeon on map {:d}", map_index);
                return 0;
            }

            character *ch = get_quest_manager().current_character_ptr();

            if (ch->get_party() == nullptr) {
                quest::error("Player is not in a party");
                return 0;
            }
            dungeon_value->jump_party(ch->get_party(), ch->map_index(), (int)lua_tonumber(lua, 2), (int)lua_tonumber(lua, 3));

            return 0;
        }

        auto dungeon_jump_all(lua_State *lua) -> int {
            if (lua_gettop(lua) < 2 || !lua_isnumber(lua, 1) || !lua_isnumber(lua, 2))
                return 0;

            dungeon *dungeon = current_dungeon();

            if (!dungeon)
                return 0;

            dungeon->jump_all(dungeon->map_index(), (int)lua_tonumber(lua, 1), (int)lua_tonumber(lua, 2));
            return 0;
        }

        auto dungeon_warp_all(lua_State *lua) -> int {
            if (lua_gettop(lua) < 2 || !lua_isnumber(lua, 1) || !lua_isnumber(lua, 2))
                return 0;

            dungeon *dungeon = current_dungeon();

            if (!dungeon)
                return 0;

            dungeon->warp_all(dungeon->map_index(), (int)lua_tonumber(lua, 1), (int)lua_tonumber(lua, 2));
            return 0;
        }

        auto dungeon_get_kill_stone_count(lua_State *lua) -> int {
            if (!lua_isnumber(lua, 1) || !lua_isnumber(lua, 2)) {
                lua_pushnumber(lua, 0);
                return 1;
            }

            dungeon *dungeon = current_dungeon();
            lua_pushnumber(lua, dungeon ? dungeon->kill_stone_count() : 0);
            return 1;
        }

        auto dungeon_get_kill_mob_count(lua_State *lua) -> int {
            if (!lua_isnumber(lua, 1) || !lua_isnumber(lua, 2)) {
                lua_pushnumber(lua, 0);
                return 1;
            }

            dungeon *dungeon = current_dungeon();
            lua_pushnumber(lua, dungeon ? dungeon->kill_mob_count() : 0);
            return 1;
        }

        auto dungeon_is_use_potion(lua_State *lua) -> int {
            if (!lua_isnumber(lua, 1) || !lua_isnumber(lua, 2)) {
                lua_pushboolean(lua, 1);
                return 1;
            }

            dungeon *dungeon = current_dungeon();
            lua_pushboolean(lua, dungeon ? dungeon->is_use_potion() : 1);
            return 1;
        }

        auto dungeon_revived(lua_State *lua) -> int {
            if (!lua_isnumber(lua, 1) || !lua_isnumber(lua, 2)) {
                lua_pushboolean(lua, 1);
                return 1;
            }

            dungeon *dungeon = current_dungeon();
            lua_pushboolean(lua, dungeon ? dungeon->is_use_revive() : 1);
            return 1;
        }

        auto dungeon_set_dest(lua_State *lua) -> int {
            if (!lua_isnumber(lua, 1) || !lua_isnumber(lua, 2))
                return 0;

            character *ch = get_quest_manager().current_character_ptr();

            party *party = ch->get_party();
            dungeon *dungeon = current_dungeon();

            if (dungeon && party)
                dungeon->send_dest_position_to_party(party, (int)lua_tonumber(lua, 1), (int)lua_tonumber(lua, 2));

            return 0;
        }

        auto dungeon_unique_set_maxhp(lua_State *lua) -> int {
            if (!lua_isstring(lua, 1) || !lua_isnumber(lua, 2))
                return 0;

            if (dungeon *dungeon = current_dungeon())
                dungeon->unique_set_max_hp(lua_tostring(lua, 1), (int)lua_tonumber(lua, 2));

            return 0;
        }

        auto dungeon_unique_set_hp(lua_State *lua) -> int {
            if (!lua_isstring(lua, 1) || !lua_isnumber(lua, 2))
                return 0;

            if (dungeon *dungeon = current_dungeon())
                dungeon->unique_set_hp(lua_tostring(lua, 1), (int)lua_tonumber(lua, 2));

            return 0;
        }

        auto dungeon_unique_set_def_grade(lua_State *lua) -> int {
            if (!lua_isstring(lua, 1) || !lua_isnumber(lua, 2))
                return 0;

            if (dungeon *dungeon = current_dungeon())
                dungeon->unique_set_def_grade(lua_tostring(lua, 1), (int)lua_tonumber(lua, 2));

            return 0;
        }

        auto dungeon_unique_get_hp_perc(lua_State *lua) -> int {
            if (!lua_isstring(lua, 1)) {
                lua_pushnumber(lua, 0);
                return 1;
            }

            dungeon *dungeon = current_dungeon();
            lua_pushnumber(lua, dungeon ? dungeon->unique_hp_perc(lua_tostring(lua, 1)) : 0);
            return 1;
        }

        auto dungeon_is_unique_dead(lua_State *lua) -> int {
            if (!lua_isstring(lua, 1)) {
                lua_pushboolean(lua, 0);
                return 1;
            }

            dungeon *dungeon = current_dungeon();
            lua_pushboolean(lua, dungeon && dungeon->is_unique_dead(lua_tostring(lua, 1)));
            return 1;
        }

        auto dungeon_purge_unique(lua_State *lua) -> int {
            if (!lua_isstring(lua, 1))
                return 0;
            metin2::log::debug("Dungeon: purge unique {}", lua_tostring(lua, 1));

            if (dungeon *dungeon = current_dungeon())
                dungeon->purge_unique(lua_tostring(lua, 1));

            return 0;
        }

        auto dungeon_purge_area(lua_State *lua) -> int {
            if (!lua_isnumber(lua, 1) || !lua_isnumber(lua, 2) || !lua_isnumber(lua, 3) || !lua_isnumber(lua, 4))
                return 0;
            metin2::log::debug("Dungeon: purge area");

            int x1 = lua_tonumber(lua, 1);
            int y1 = lua_tonumber(lua, 2);
            int x2 = lua_tonumber(lua, 3);
            int y2 = lua_tonumber(lua, 4);

            dungeon *dungeon = current_dungeon_or_error();
            if (!dungeon)
                return 0;

            const int map_index = dungeon->map_index();

            if (0 == map_index) {
                quest::error("No map at ({:d}, {:d})", x1, y1);
                return 0;
            }

            sectree_map *sectree = get_sectree_manager().map(map_index);

            if (nullptr != sectree) {
                purge_monsters_in_area(sectree, x1, y1, x2, y2, get_quest_manager().current_npc_character_ptr());
            }

            return 0;
        }

        auto dungeon_kill_unique(lua_State *lua) -> int {
            if (!lua_isstring(lua, 1))
                return 0;
            metin2::log::debug("Dungeon: kill unique {}", lua_tostring(lua, 1));

            if (dungeon *dungeon = current_dungeon())
                dungeon->kill_unique(lua_tostring(lua, 1));

            return 0;
        }

        auto dungeon_spawn_stone_door(lua_State *lua) -> int {
            if (!lua_isstring(lua, 1) || !lua_isstring(lua, 2))
                return 0;
            metin2::log::debug("Dungeon: spawn stone door {} at {}", lua_tostring(lua, 1), lua_tostring(lua, 2));

            if (dungeon *dungeon = current_dungeon())
                dungeon->spawn_stone_door(lua_tostring(lua, 1), lua_tostring(lua, 2));

            return 0;
        }

        auto dungeon_spawn_wooden_door(lua_State *lua) -> int {
            if (!lua_isstring(lua, 1) || !lua_isstring(lua, 2))
                return 0;
            metin2::log::debug("Dungeon: spawn wooden door {} at {}", lua_tostring(lua, 1), lua_tostring(lua, 2));

            if (dungeon *dungeon = current_dungeon())
                dungeon->spawn_wooden_door(lua_tostring(lua, 1), lua_tostring(lua, 2));

            return 0;
        }

        auto dungeon_spawn_move_group(lua_State *lua) -> int {
            if (!lua_isnumber(lua, 1) || !lua_isstring(lua, 2) || !lua_isstring(lua, 3))
                return 0;
            metin2::log::debug("Dungeon: spawn moving group (vnum {:d}) from {} to {}", (int)lua_tonumber(lua, 1), lua_tostring(lua, 2), lua_tostring(lua, 3));

            if (dungeon *dungeon = current_dungeon())
                dungeon->spawn_move_group((uint32_t)lua_tonumber(lua, 1), lua_tostring(lua, 2), lua_tostring(lua, 3));

            return 0;
        }

        auto dungeon_spawn_move_unique(lua_State *lua) -> int {
            if (!lua_isstring(lua, 1) || !lua_isnumber(lua, 2) || !lua_isstring(lua, 3) || !lua_isstring(lua, 4))
                return 0;
            metin2::log::debug("Dungeon: spawn moving unique {} (vnum {:d}) from {} to {}", lua_tostring(lua, 1), (int)lua_tonumber(lua, 2), lua_tostring(lua, 3), lua_tostring(lua, 4));

            if (dungeon *dungeon = current_dungeon())
                dungeon->spawn_move_unique(lua_tostring(lua, 1), (uint32_t)lua_tonumber(lua, 2), lua_tostring(lua, 3), lua_tostring(lua, 4));

            return 0;
        }

        auto dungeon_spawn_unique(lua_State *lua) -> int {
            if (!lua_isstring(lua, 1) || !lua_isnumber(lua, 2) || !lua_isstring(lua, 3))
                return 0;
            metin2::log::debug("Dungeon: spawn unique {} (vnum {:d}) at {}", lua_tostring(lua, 1), (int)lua_tonumber(lua, 2), lua_tostring(lua, 3));

            if (dungeon *dungeon = current_dungeon())
                dungeon->spawn_unique(lua_tostring(lua, 1), (uint32_t)lua_tonumber(lua, 2), lua_tostring(lua, 3));

            return 0;
        }

        auto dungeon_spawn(lua_State *lua) -> int {
            if (!lua_isnumber(lua, 1) || !lua_isstring(lua, 2))
                return 0;
            metin2::log::debug("Dungeon: spawn mob {:d} at {}", (int)lua_tonumber(lua, 1), lua_tostring(lua, 2));

            if (dungeon *dungeon = current_dungeon())
                dungeon->spawn((uint32_t)lua_tonumber(lua, 1), lua_tostring(lua, 2));

            return 0;
        }

        auto dungeon_set_unique(lua_State *lua) -> int {
            if (!lua_isstring(lua, 1) || !lua_isnumber(lua, 2))
                return 0;

            dungeon *dungeon = current_dungeon();

            auto vid = (uint32_t)lua_tonumber(lua, 2);

            if (dungeon)
                dungeon->set_unique(lua_tostring(lua, 1), vid);
            return 0;
        }

        auto dungeon_get_unique_vid(lua_State *lua) -> int {
            if (!lua_isstring(lua, 1)) {
                lua_pushnumber(lua, 0);
                return 1;
            }

            dungeon *dungeon = current_dungeon();
            lua_pushnumber(lua, dungeon ? dungeon->unique_vid(lua_tostring(lua, 1)) : 0);
            return 1;
        }

        auto dungeon_spawn_mob(lua_State *lua) -> int {
            if (!lua_isnumber(lua, 1) || !lua_isnumber(lua, 2) || !lua_isnumber(lua, 3)) {
                quest::error("Invalid arguments");
                return 0;
            }

            dungeon *dungeon = current_dungeon();

            uint32_t vid = 0;

            if (dungeon) {
                auto vnum = (uint32_t)lua_tonumber(lua, 1);
                auto x = (int32_t)lua_tonumber(lua, 2);
                auto y = (int32_t)lua_tonumber(lua, 3);
                float radius = lua_isnumber(lua, 4) ? (float)lua_tonumber(lua, 4) : 0;
                uint32_t count = (lua_isnumber(lua, 5)) ? (uint32_t)lua_tonumber(lua, 5) : 1;

                metin2::log::debug("Dungeon: spawn mob {:d} at ({:d}, {:d})", vnum, x, y);

                if (count == 0)
                    count = 1;

                while (count--) {
                    if (radius < 1) {
                        character *ch = dungeon->spawn_mob(vnum, x, y);
                        if (ch && !vid)
                            vid = ch->get_vid();
                    } else {
                        float angle = number(0, 999) * std::numbers::pi * 2 / 1000;
                        float r = number(0, 999) * radius / 1000;

                        int32_t nx = x + (int32_t)(r * cos(angle));
                        int32_t ny = y + (int32_t)(r * sin(angle));

                        character *ch = dungeon->spawn_mob(vnum, nx, ny);
                        if (ch && !vid)
                            vid = ch->get_vid();
                    }
                }
            }

            lua_pushnumber(lua, vid);
            return 1;
        }

        auto dungeon_spawn_mob_dir(lua_State *lua) -> int {
            if (!lua_isnumber(lua, 1) || !lua_isnumber(lua, 2) || !lua_isnumber(lua, 3) || !lua_isnumber(lua, 4)) {
                quest::error("Invalid arguments");
                return 0;
            }

            dungeon *dungeon = current_dungeon();

            uint32_t vid = 0;

            if (dungeon) {
                auto vnum = (uint32_t)lua_tonumber(lua, 1);
                auto x = (int32_t)lua_tonumber(lua, 2);
                auto y = (int32_t)lua_tonumber(lua, 3);
                uint8_t dir = (int)lua_tonumber(lua, 4);

                character *ch = dungeon->spawn_mob(vnum, x, y, dir);
                if (ch && !vid)
                    vid = ch->get_vid();
            }
            lua_pushnumber(lua, vid);
            return 1;
        }

        auto dungeon_spawn_mob_ac_dir(lua_State *lua) -> int {
            if (!lua_isnumber(lua, 1) || !lua_isnumber(lua, 2) || !lua_isnumber(lua, 3) || !lua_isnumber(lua, 4)) {
                quest::error("Invalid arguments");
                return 0;
            }

            dungeon *dungeon = current_dungeon();

            uint32_t vid = 0;

            if (dungeon) {
                auto vnum = (uint32_t)lua_tonumber(lua, 1);
                auto x = (int32_t)lua_tonumber(lua, 2);
                auto y = (int32_t)lua_tonumber(lua, 3);
                uint8_t dir = (int)lua_tonumber(lua, 4);

                character *ch = dungeon->spawn_mob_ac_dir(vnum, x, y, dir);
                if (ch && !vid)
                    vid = ch->get_vid();
            }
            lua_pushnumber(lua, vid);
            return 1;
        }

        auto dungeon_spawn_goto_mob(lua_State *lua) -> int {
            if (!lua_isnumber(lua, 1) || !lua_isnumber(lua, 2) || !lua_isnumber(lua, 3) || !lua_isnumber(lua, 4))
                return 0;

            auto from_x = (int32_t)lua_tonumber(lua, 1);
            auto from_y = (int32_t)lua_tonumber(lua, 2);
            auto to_x = (int32_t)lua_tonumber(lua, 3);
            auto to_y = (int32_t)lua_tonumber(lua, 4);

            if (dungeon *dungeon = current_dungeon())
                dungeon->spawn_goto_mob(from_x, from_y, to_x, to_y);

            return 0;
        }

        auto dungeon_spawn_name_mob(lua_State *lua) -> int {
            if (!lua_isnumber(lua, 1) || !lua_isnumber(lua, 2) || !lua_isnumber(lua, 3) || !lua_isstring(lua, 4))
                return 0;

            dungeon *dungeon = current_dungeon();

            if (dungeon) {
                auto vnum = (uint32_t)lua_tonumber(lua, 1);
                auto x = (int32_t)lua_tonumber(lua, 2);
                auto y = (int32_t)lua_tonumber(lua, 3);
                dungeon->spawn_name_mob(vnum, x, y, lua_tostring(lua, 4));
            }
            return 0;
        }

        auto dungeon_spawn_group(lua_State *lua) -> int {
            if (!lua_isnumber(lua, 1) || !lua_isnumber(lua, 2) || !lua_isnumber(lua, 3) || !lua_isnumber(lua, 4) || !lua_isnumber(lua, 6)) {
                quest::error("Invalid arguments");
                return 0;
            }

            uint32_t vid = 0;

            dungeon *dungeon = current_dungeon();

            if (dungeon) {
                auto group_vnum = (uint32_t)lua_tonumber(lua, 1);
                int32_t local_x = (int32_t)lua_tonumber(lua, 2) * 100;
                int32_t local_y = (int32_t)lua_tonumber(lua, 3) * 100;
                float radius = (float)lua_tonumber(lua, 4) * 100;
                bool aggressive = lua_toboolean(lua, 5);
                auto count = (uint32_t)lua_tonumber(lua, 6);

                character *ch_ret = dungeon->spawn_group(group_vnum, local_x, local_y, radius, aggressive, count);
                if (ch_ret)
                    vid = ch_ret->get_vid();
            }

            lua_pushnumber(lua, vid);
            return 1;
        }

        auto dungeon_join(lua_State *lua) -> int {
            if (lua_gettop(lua) < 1 || !lua_isnumber(lua, 1))
                return 0;

            auto map_index = (int32_t)lua_tonumber(lua, 1);
            dungeon *dungeon = get_dungeon_manager().create(map_index);

            if (!dungeon)
                return 0;

            character *ch = get_quest_manager().current_character_ptr();

            if (ch->get_party() && ch->get_party()->leader_pid() == ch->player_id())
                dungeon->join_party(ch->get_party());
            else if (!ch->get_party())
                dungeon->join(ch);

            return 0;
        }

        auto dungeon_exit(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();

            ch->exit_to_saved_location();
            return 0;
        }

        auto dungeon_exit_all(lua_State *lua) -> int {
            if (dungeon *dungeon = current_dungeon())
                dungeon->exit_all();

            return 0;
        }

        auto dungeon_say_diff_by_item_group(lua_State *lua) -> int {
            if (!lua_isstring(lua, 1) || !lua_isstring(lua, 2) || !lua_isstring(lua, 3)) {
                metin2::log::error("Invalid arguments");
                return 0;
            }

            auto target = current_item_group(lua_tostring(lua, 1));
            if (!target)
                return 0;
            const dungeon::item_group *item_group = target->group;

            std::string can_enter_ment = lua_tostring(lua, 2);
            can_enter_ment += "[ENTER][ENTER][ENTER][ENTER][DONE]";
            std::string cant_enter_ment = lua_tostring(lua, 3);
            cant_enter_ment += "[ENTER][ENTER][ENTER][ENTER][DONE]";

            target->map->for_each([&](entity *ent) {
                if (ent->is_type(ENTITY_CHARACTER)) {
                    auto *ch = (character *)ent;

                    if (ch->is_pc()) {
                        struct metin2::game::packet_script packet_script;
                        temp_buffer buf;

                        for (auto it = item_group->begin(); it != item_group->end(); it++) {
                            if (ch->count_specify_item(it->first) >= it->second) {
                                packet_script.header = HEADER_GC_SCRIPT;
                                packet_script.skin = quest::quest_manager::QUEST_SKIN_NORMAL;
                                packet_script.src_size = can_enter_ment.size();
                                packet_script.size = packet_script.src_size + sizeof(struct game::protocol::packet_script);

                                buf.write(&packet_script, sizeof(struct game::protocol::packet_script));
                                buf.write(&can_enter_ment[0], can_enter_ment.size());
                                ch->get_desc()->packet(buf.read_peek(), buf.size());
                                return;
                            }
                        }

                        packet_script.header = HEADER_GC_SCRIPT;
                        packet_script.skin = quest::quest_manager::QUEST_SKIN_NORMAL;
                        packet_script.src_size = cant_enter_ment.size();
                        packet_script.size = packet_script.src_size + sizeof(struct game::protocol::packet_script);

                        buf.write(&packet_script, sizeof(struct game::protocol::packet_script));
                        buf.write(&cant_enter_ment[0], cant_enter_ment.size());
                        ch->get_desc()->packet(buf.read_peek(), buf.size());
                    }
                }
            });

            return 0;
        }

        auto dungeon_exit_all_by_item_group(lua_State *lua) -> int {
            if (!lua_isstring(lua, 1)) {
                metin2::log::error("Invalid arguments");
                return 0;
            }

            auto target = current_item_group(lua_tostring(lua, 1));
            if (!target)
                return 0;
            const dungeon::item_group *item_group = target->group;

            target->map->for_each([&](entity *ent) {
                if (ent->is_type(ENTITY_CHARACTER)) {
                    auto *ch = (character *)ent;

                    if (ch->is_pc()) {
                        for (auto it = item_group->begin(); it != item_group->end(); it++) {
                            if (ch->count_specify_item(it->first) >= it->second) {
                                return;
                            }
                        }
                        ch->exit_to_saved_location();
                    }
                }
            });

            return 0;
        }

        auto dungeon_delete_item_in_item_group_from_all(lua_State *lua) -> int {
            if (!lua_isstring(lua, 1)) {
                metin2::log::error("Invalid arguments");
                return 0;
            }

            auto target = current_item_group(lua_tostring(lua, 1));
            if (!target)
                return 0;
            const dungeon::item_group *item_group = target->group;

            target->map->for_each([&](entity *ent) {
                if (ent->is_type(ENTITY_CHARACTER)) {
                    auto *ch = (character *)ent;

                    if (ch->is_pc()) {
                        for (auto it = item_group->begin(); it != item_group->end(); it++) {
                            if (ch->count_specify_item(it->first) >= it->second) {
                                ch->remove_specify_item(it->first, it->second);
                                return;
                            }
                        }
                    }
                }
            });

            return 0;
        }

        auto dungeon_kill_all(lua_State *lua) -> int {
            if (dungeon *dungeon = current_dungeon())
                dungeon->kill_all();

            return 0;
        }

        auto dungeon_purge(lua_State *lua) -> int {
            if (dungeon *dungeon = current_dungeon())
                dungeon->purge();

            return 0;
        }

        auto dungeon_exit_all_to_start_position(lua_State *lua) -> int {
            if (dungeon *dungeon = current_dungeon())
                dungeon->exit_all_to_start_position();

            return 0;
        }

        auto dungeon_count_monster(lua_State *lua) -> int {
            dungeon *dungeon = current_dungeon_or_error();
            lua_pushnumber(lua, dungeon ? dungeon->count_monster() : INT32_MAX);
            return 1;
        }

        auto dungeon_select(lua_State *lua) -> int {
            auto map_index = (uint32_t)lua_tonumber(lua, 1);
            if (map_index) {
                dungeon *dungeon = get_dungeon_manager().find_by_map_index(map_index);
                if (dungeon) {
                    get_quest_manager().select_dungeon(dungeon);
                    lua_pushboolean(lua, 1);
                } else {
                    get_quest_manager().select_dungeon(nullptr);
                    lua_pushboolean(lua, 0);
                }
            } else {
                get_quest_manager().select_dungeon(nullptr);
                lua_pushboolean(lua, 0);
            }
            return 1;
        }

        auto dungeon_find(lua_State *lua) -> int {
            auto map_index = (uint32_t)lua_tonumber(lua, 1);
            if (map_index) {
                dungeon *dungeon = get_dungeon_manager().find_by_map_index(map_index);
                if (dungeon) {
                    lua_pushboolean(lua, 1);
                } else {
                    lua_pushboolean(lua, 0);
                }
            } else {
                lua_pushboolean(lua, 0);
            }
            return 1;
        }

        auto dungeon_all_near_to(lua_State *lua) -> int {
            dungeon *dungeon = current_dungeon();

            if (dungeon != nullptr) {
                lua_pushboolean(lua, dungeon->is_all_pc_near_to((int)lua_tonumber(lua, 1), (int)lua_tonumber(lua, 2), 30));
            } else {
                lua_pushboolean(lua, false);
            }

            return 1;
        }

        auto dungeon_set_warp_location(lua_State *lua) -> int {
            dungeon *dungeon = current_dungeon();

            if (dungeon == nullptr) {
                return 0;
            }

            if (lua_gettop(lua) < 3 || !lua_isnumber(lua, 1) || !lua_isnumber(lua, 2) || !lua_isnumber(lua, 3)) {
                return 0;
            }

            auto map_index = (int)lua_tonumber(lua, 1);
            auto x = (int)lua_tonumber(lua, 2);
            auto y = (int)lua_tonumber(lua, 3);
            dungeon->for_each_member([&](character *ch) {
                if (ch->is_pc()) {
                    ch->set_warp_location(map_index, x, y);
                }
            });

            return 0;
        }

        auto dungeon_set_item_group(lua_State *lua) -> int {
            if (!lua_isstring(lua, 1) || !lua_isnumber(lua, 2)) {
                return 0;
            }
            std::string group_name(lua_tostring(lua, 1));
            int size = lua_tonumber(lua, 2);

            dungeon::item_group item_group;

            for (int i = 0; i < size; i++) {
                if (!lua_isnumber(lua, i * 2 + 3) || !lua_isnumber(lua, i * 2 + 4)) {
                    return 0;
                }
                item_group.push_back(std::pair<uint32_t, int>(lua_tonumber(lua, i * 2 + 3), lua_tonumber(lua, i * 2 + 4)));
            }
            dungeon *dungeon = current_dungeon();

            if (dungeon == nullptr) {
                return 0;
            }

            dungeon->create_item_group(group_name, item_group);
            return 0;
        }

        auto dungeon_set_quest_flag2(lua_State *lua) -> int {
            if (!lua_isstring(lua, 1) || !lua_isstring(lua, 2) || !lua_isnumber(lua, 3)) {
                quest::error("Invalid arguments");
            }

            std::string flagname = std::string(lua_tostring(lua, 1)) + "." + lua_tostring(lua, 2);
            set_members_quest_flag(flagname, (int)rint(lua_tonumber(lua, 3)));
            return 0;
        }

        void register_dungeon_function_table() {
            luaL_reg dungeon_functions[] =
                {
                    {"join",                               dungeon_join                              },
                    {"exit",                               dungeon_exit                              },
                    {"exit_all",                           dungeon_exit_all                          },
                    {"set_item_group",                     dungeon_set_item_group                    },
                    {"exit_all_by_item_group",             dungeon_exit_all_by_item_group            },
                    {"say_diff_by_item_group",             dungeon_say_diff_by_item_group            },
                    {"delete_item_in_item_group_from_all", dungeon_delete_item_in_item_group_from_all},
                    {"purge",                              dungeon_purge                             },
                    {"kill_all",                           dungeon_kill_all                          },
                    {"spawn",                              dungeon_spawn                             },
                    {"spawn_mob",                          dungeon_spawn_mob                         },
                    {"spawn_mob_dir",                      dungeon_spawn_mob_dir                     },
                    {"spawn_mob_ac_dir",                   dungeon_spawn_mob_ac_dir                  },
                    {"spawn_name_mob",                     dungeon_spawn_name_mob                    },
                    {"spawn_goto_mob",                     dungeon_spawn_goto_mob                    },
                    {"spawn_group",                        dungeon_spawn_group                       },
                    {"spawn_unique",                       dungeon_spawn_unique                      },
                    {"spawn_move_unique",                  dungeon_spawn_move_unique                 },
                    {"spawn_move_group",                   dungeon_spawn_move_group                  },
                    {"spawn_stone_door",                   dungeon_spawn_stone_door                  },
                    {"spawn_wooden_door",                  dungeon_spawn_wooden_door                 },
                    {"purge_unique",                       dungeon_purge_unique                      },
                    {"purge_area",                         dungeon_purge_area                        },
                    {"kill_unique",                        dungeon_kill_unique                       },
                    {"is_unique_dead",                     dungeon_is_unique_dead                    },
                    {"unique_get_hp_perc",                 dungeon_unique_get_hp_perc                },
                    {"unique_set_def_grade",               dungeon_unique_set_def_grade              },
                    {"unique_set_hp",                      dungeon_unique_set_hp                     },
                    {"unique_set_maxhp",                   dungeon_unique_set_maxhp                  },
                    {"get_unique_vid",                     dungeon_get_unique_vid                    },
                    {"get_kill_stone_count",               dungeon_get_kill_stone_count              },
                    {"get_kill_mob_count",                 dungeon_get_kill_mob_count                },
                    {"is_use_potion",                      dungeon_is_use_potion                     },
                    {"revived",                            dungeon_revived                           },
                    {"set_dest",                           dungeon_set_dest                          },
                    {"jump_all",                           dungeon_jump_all                          },
                    {"warp_all",                           dungeon_warp_all                          },
                    {"new_jump_all",                       dungeon_new_jump_all                      },
                    {"new_jump_party",                     dungeon_new_jump_party                    },
                    {"new_jump",                           dungeon_new_jump                          },
                    {"regen_file",                         dungeon_regen_file                        },
                    {"set_regen_file",                     dungeon_set_regen_file                    },
                    {"clear_regen",                        dungeon_clear_regen                       },
                    {"set_exit_all_at_eliminate",          dungeon_set_exit_all_at_eliminate         },
                    {"set_warp_at_eliminate",              dungeon_set_warp_at_eliminate             },
                    {"get_map_index",                      dungeon_get_map_index                     },
                    {"check_eliminated",                   dungeon_check_eliminated                  },
                    {"exit_all_to_start_position",         dungeon_exit_all_to_start_position        },
                    {"count_monster",                      dungeon_count_monster                     },
                    {"setf",                               dungeon_set_flag                          },
                    {"getf",                               dungeon_get_flag                          },
                    {"getf_from_map_index",                dungeon_get_flag_from_map_index           },
                    {"set_unique",                         dungeon_set_unique                        },
                    {"select",                             dungeon_select                            },
                    {"find",                               dungeon_find                              },
                    {"notice",                             dungeon_notice                            },
                    {"setqf",                              dungeon_set_quest_flag                    },
                    {"all_near_to",                        dungeon_all_near_to                       },
                    {"set_warp_location",                  dungeon_set_warp_location                 },
                    {"setqf2",                             dungeon_set_quest_flag2                   },

                    {nullptr,                              nullptr                                   }
            };

            get_quest_manager().add_lua_function_table("d", dungeon_functions);
        }
    }
}
