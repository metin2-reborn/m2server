
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/events/arena.h>
#include <metin2/game/quest/lua_incl.h>
#include <metin2/game/quest/questmanager.h>

#include <metin2/log/log.h>

#include <cstdint>

namespace metin2::game {

    namespace quest {
        auto arena_start_duel(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            character *ch2 = get_character_manager().find_pc(lua_tostring(lua, 1));
            int set_point = (int)lua_tonumber(lua, 2);

            if (ch == nullptr || ch2 == nullptr) {
                lua_pushnumber(lua, 0);
                return 1;
            }

            if (ch->is_horse_riding() == true) {
                ch->stop_riding();
                ch->horse_summon(false);
            }

            if (ch2->is_horse_riding() == true) {
                ch2->stop_riding();
                ch2->horse_summon(false);
            }

            if (get_arena_manager().is_member(ch->map_index(), ch->player_id()) != MEMBER_NO ||
                get_arena_manager().is_member(ch2->map_index(), ch2->player_id()) != MEMBER_NO) {
                lua_pushnumber(lua, 2);
                return 1;
            }
            if (get_arena_manager().start_duel(ch, ch2, set_point) == false) {
                lua_pushnumber(lua, 3);
                return 1;
            }

            lua_pushnumber(lua, 1);

            return 1;
        }

        auto arena_add_map(lua_State *lua) -> int {
            int map_idx = (int)lua_tonumber(lua, 1);
            int startpos_ax = (int)lua_tonumber(lua, 2);
            int startpos_ay = (int)lua_tonumber(lua, 3);
            int startpos_bx = (int)lua_tonumber(lua, 4);
            int startpos_by = (int)lua_tonumber(lua, 5);

            if (get_arena_manager().add_arena(map_idx, startpos_ax, startpos_ay, startpos_bx, startpos_by) == false) {
                metin2::log::error("Cannot add arena on map {:d} (start A {:d},{:d}, start B {:d},{:d})", map_idx, startpos_ax, startpos_ay, startpos_bx, startpos_by);
            } else {
                metin2::log::info("Arena: added map {:d} (start A {:d},{:d}, start B {:d},{:d})", map_idx, startpos_ax, startpos_ay, startpos_bx, startpos_by);
            }

            return 1;
        }

        auto arena_get_duel_list(lua_State *lua) -> int {
            get_arena_manager().duel_list(lua);

            return 1;
        }

        auto arena_add_observer(lua_State *lua) -> int {
            int map_idx = (int)lua_tonumber(lua, 1);
            int ob_point_x = (int)lua_tonumber(lua, 2);
            int ob_point_y = (int)lua_tonumber(lua, 3);
            character *ch = get_quest_manager().current_character_ptr();

            get_arena_manager().add_observer(ch, map_idx, ob_point_x, ob_point_y);

            return 1;
        }

        auto arena_is_in_arena(lua_State *lua) -> int {
            auto pid = (uint32_t)lua_tonumber(lua, 1);

            character *ch = get_character_manager().find_by_pid(pid);

            if (ch == nullptr) {
                lua_pushnumber(lua, 1);
            } else {
                if (ch->get_arena() == nullptr || ch->arena_observer_mode() == true) {
                    if (get_arena_manager().is_member(ch->map_index(), ch->player_id()) == MEMBER_DUELIST)
                        lua_pushnumber(lua, 1);
                    else
                        lua_pushnumber(lua, 0);
                } else {
                    lua_pushnumber(lua, 0);
                }
            }
            return 1;
        }

        void register_arena_function_table() {
            luaL_reg arena_functions[] =
                {
                    {"start_duel",    arena_start_duel   },
                    {"add_map",       arena_add_map      },
                    {"get_duel_list", arena_get_duel_list},
                    {"add_observer",  arena_add_observer },
                    {"is_in_arena",   arena_is_in_arena  },

                    {nullptr,         nullptr            }
            };

            get_quest_manager().add_lua_function_table("arena", arena_functions);
        }
    }
}
