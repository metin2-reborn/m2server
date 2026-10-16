#include <metin2/game/app/utils.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/quest/lua_incl.h>
#include <metin2/game/quest/questlua.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/social/marriage.h>
#include <metin2/game/social/wedding.h>

#include <metin2/proto/chat_types.h>

#include <cstdint>

namespace metin2::game {

    namespace quest {
        auto marriage_engage_to(lua_State *lua) -> int {
            uint32_t vid = (uint32_t)lua_tonumber(lua, 1);
            character *ch = quest_manager::instance().current_character_ptr();
            character *ch_you = character_manager::instance().find(vid);
            if (ch_you) {
                marriage::manager::instance().request_add(ch->player_id(), ch_you->player_id(), ch->name(), ch_you->name());
            }
            return 0;
        }

        auto marriage_remove(lua_State *lua) -> int {
            character *ch = quest_manager::instance().current_character_ptr();
            marriage::marriage *pMarriage = marriage::manager::instance().get(ch->player_id());
            if (!pMarriage) {
                quest::error("Player {1} (pid {0:d}) is not married", ch->player_id(), ch->name());
                return 0;
            }
            marriage::manager::instance().request_remove(ch->player_id(), pMarriage->other(ch->player_id()));
            return 0;
        }

        auto marriage_set_to_marriage(lua_State *lua) -> int {
            character *ch = quest_manager::instance().current_character_ptr();
            marriage::marriage *pMarriage = marriage::manager::instance().get(ch->player_id());
            if (!pMarriage) {
                quest::error("Player {1} (pid {0:d}) is not married", ch->player_id(), ch->name());
                return 0;
            }
            pMarriage->set_married();
            return 0;
        }

        auto marriage_find_married_vid(lua_State *lua) -> int {
            character *ch = quest_manager::instance().current_character_ptr();
            marriage::marriage *pMarriage = marriage::manager::instance().get(ch->player_id());
            uint32_t vid = 0;
            if (pMarriage) {
                character *you = character_manager::instance().find_by_pid(pMarriage->other(ch->player_id()));
                if (you)
                    vid = you->get_vid();
            }

            lua_pushnumber(lua, vid);

            return 1;
        }

        auto marriage_get_wedding_list(lua_State *lua) -> int {
            int count = 1;
            lua_newtable(lua);
            marriage::manager::instance().for_each_wedding([&](marriage::marriage *marriage_value) {
                if (!marriage_value->wedding_info)
                    return;

                lua_newtable(lua);
                lua_pushnumber(lua, marriage_value->pid1);
                lua_rawseti(lua, -2, 1);
                lua_pushnumber(lua, marriage_value->pid2);
                lua_rawseti(lua, -2, 2);
                lua_pushstring(lua, marriage_value->name1.c_str());
                lua_rawseti(lua, -2, 3);
                lua_pushstring(lua, marriage_value->name2.c_str());
                lua_rawseti(lua, -2, 4);
                lua_rawseti(lua, -2, count++);
            });
            return 1;
        }

        auto marriage_join_wedding(lua_State *lua) -> int {
            if (!lua_isnumber(lua, 1) || !lua_isnumber(lua, 2)) {
                quest::error("Invalid player id arguments");
                return 0;
            }

            uint32_t pid1 = (uint32_t)lua_tonumber(lua, 1);
            uint32_t pid2 = (uint32_t)lua_tonumber(lua, 2);

            character *ch = quest_manager::instance().current_character_ptr();

            marriage::marriage *pMarriage = marriage::manager::instance().get(pid1);
            if (!pMarriage) {
                quest::error("Player {1} (pid {0:d}) is not married", ch->player_id(), ch->name());
                return 0;
            }
            if (pMarriage->other(pid1) != pid2) {
                quest::error("Players (pid {:d}, pid {:d}) are not married to each other", pid1, pid2);
                return 0;
            }
            if (ch->is_hack())
                return 0;

            pMarriage->warp_to_wedding_map(ch->player_id());
            return 0;
        }

        auto marriage_warp_to_my_marriage_map(lua_State *lua) -> int {
            character *ch = quest_manager::instance().current_character_ptr();
            marriage::marriage *pMarriage = marriage::manager::instance().get(ch->player_id());
            if (!pMarriage) {
                quest::error("Player {1} (pid {0:d}) is not married", ch->player_id(), ch->name());
                return 0;
            }

            if (ch->is_hack())
                return 0;

            pMarriage->warp_to_wedding_map(ch->player_id());
            return 0;
        }

        auto marriage_end_wedding(lua_State *lua) -> int {
            character *ch = quest_manager::instance().current_character_ptr();
            marriage::marriage *pMarriage = marriage::manager::instance().get(ch->player_id());
            if (!pMarriage) {
                quest::error("Player {1} (pid {0:d}) is not married", ch->player_id(), ch->name());
                return 0;
            }
            if (pMarriage->wedding_info) {
                pMarriage->request_end_wedding();
            }
            return 0;
        }

        auto marriage_wedding_dark(lua_State *lua) -> int {
            if (!lua_isboolean(lua, 1)) {
                quest::error("Argument 1 must be a boolean");
                return 0;
            }
            character *ch = quest_manager::instance().current_character_ptr();
            marriage::marriage *pMarriage = marriage::manager::instance().get(ch->player_id());

            if (!pMarriage) {
                quest::error("Player {1} (pid {0:d}) is not married", ch->player_id(), ch->name());
                return 0;
            }
            if (pMarriage->wedding_info) {
                marriage::wedding_map *pWedding = marriage::wedding_manager::instance().find(pMarriage->wedding_info->map_index);
                pWedding->set_dark(lua_toboolean(lua, 1));
            }

            return 0;
        }

        auto marriage_wedding_client_command(lua_State *lua) -> int {
            if (!require_string(lua, 1, "Argument 1 must be a string"))
                return 0;

            character *ch = quest_manager::instance().current_character_ptr();
            marriage::marriage *pMarriage = marriage::manager::instance().get(ch->player_id());
            if (!pMarriage) {
                quest::error("Player {1} (pid {0:d}) is not married", ch->player_id(), ch->name());
                return 0;
            }
            if (pMarriage->wedding_info) {
                marriage::wedding_map *pWedding = marriage::wedding_manager::instance().find(pMarriage->wedding_info->map_index);
                pWedding->shout_in_map(CHAT_TYPE_COMMAND, lua_tostring(lua, 1));
            }
            return 0;
        }

        auto marriage_wedding_is_playing_music(lua_State *lua) -> int {
            character *ch = quest_manager::instance().current_character_ptr();
            marriage::marriage *pMarriage = marriage::manager::instance().get(ch->player_id());
            if (!pMarriage) {
                quest::error("Player {1} (pid {0:d}) is not married", ch->player_id(), ch->name());
                return 0;
            }
            if (pMarriage->wedding_info) {
                marriage::wedding_map *pWedding = marriage::wedding_manager::instance().find(pMarriage->wedding_info->map_index);
                if (pWedding)
                    lua_pushboolean(lua, pWedding->is_playing_music());
                else
                    lua_pushboolean(lua, false);
            }

            lua_pushboolean(lua, false);
            return 1;
        }
        auto marriage_wedding_music(lua_State *lua) -> int {
            if (!lua_isboolean(lua, 1)) {
                quest::error("Argument 1 must be a boolean");
                return 0;
            }
            if (!require_string(lua, 2, "Argument 2 must be a string"))
                return 0;

            character *ch = quest_manager::instance().current_character_ptr();
            marriage::marriage *pMarriage = marriage::manager::instance().get(ch->player_id());
            if (!pMarriage) {
                quest::error("Player {1} (pid {0:d}) is not married", ch->player_id(), ch->name());
                return 0;
            }
            if (pMarriage->wedding_info) {
                marriage::wedding_map *pWedding = marriage::wedding_manager::instance().find(pMarriage->wedding_info->map_index);
                pWedding->set_music(
                    lua_toboolean(lua, 1),
                    lua_tostring(lua, 2));
            }
            return 0;
        }
        auto marriage_wedding_snow(lua_State *lua) -> int {
            if (!lua_isboolean(lua, 1)) {
                quest::error("Argument 1 must be a boolean");
                return 0;
            }
            character *ch = quest_manager::instance().current_character_ptr();
            marriage::marriage *pMarriage = marriage::manager::instance().get(ch->player_id());
            if (!pMarriage) {
                quest::error("Player {1} (pid {0:d}) is not married", ch->player_id(), ch->name());
                return 0;
            }
            if (pMarriage->wedding_info) {
                marriage::wedding_map *pWedding = marriage::wedding_manager::instance().find(pMarriage->wedding_info->map_index);
                pWedding->set_snow(lua_toboolean(lua, 1));
            }
            return 0;
        }

        auto marriage_in_my_wedding(lua_State *lua) -> int {
            character *ch = quest_manager::instance().current_character_ptr();
            marriage::marriage *pMarriage = marriage::manager::instance().get(ch->player_id());
            if (pMarriage->wedding_info) {
                lua_pushboolean(lua, (uint32_t)ch->map_index() == pMarriage->wedding_info->map_index);
            } else {
                lua_pushboolean(lua, 0);
            }
            return 1;
        }

        auto marriage_get_married_time(lua_State *lua) -> int {
            character *ch = quest_manager::instance().current_character_ptr();
            marriage::marriage *pMarriage = marriage::manager::instance().get(ch->player_id());

            if (!pMarriage) {
                quest::error("Player {} is not married", ch->name());
                lua_pushnumber(lua, 0);
                return 1;
            }

            lua_pushnumber(lua, get_global_time() - pMarriage->marry_time);
            return 1;
        }

        void register_marriage_function_table() {
            luaL_reg marriage_functions[] =
                {
                    {"engage_to",                marriage_engage_to               },
                    {"remove",                   marriage_remove                  },
                    {"find_married_vid",         marriage_find_married_vid        },
                    {"get_wedding_list",         marriage_get_wedding_list        },
                    {"join_wedding",             marriage_join_wedding            },
                    {"set_to_marriage",          marriage_set_to_marriage         },
                    {"end_wedding",              marriage_end_wedding             },
                    {"wedding_dark",             marriage_wedding_dark            },
                    {"wedding_snow",             marriage_wedding_snow            },
                    {"wedding_music",            marriage_wedding_music           },
                    {"wedding_is_playing_music", marriage_wedding_is_playing_music},
                    {"wedding_client_command",   marriage_wedding_client_command  },
                    {"in_my_wedding",            marriage_in_my_wedding           },
                    {"warp_to_my_marriage_map",  marriage_warp_to_my_marriage_map },
                    {"get_married_time",         marriage_get_married_time        },
                    {nullptr,                    nullptr                          }
            };
            quest_manager::instance().add_lua_function_table("marriage", marriage_functions);
        }
    }
}
