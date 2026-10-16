#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/quest/lua_incl.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/quest/target.h>
#include <metin2/game/world/sectree_manager.h>

#include <metin2/log/log.h>

#include <cstdint>

namespace metin2::game {

    namespace quest {
        auto target_pos(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            int quest_index = get_quest_manager().current_pc()->current_quest_index();

            if (!lua_isstring(lua, 1) || !lua_isnumber(lua, 2) || !lua_isnumber(lua, 3)) {
                metin2::log::error("Invalid arguments from player {} (quest index {:d})", ch->name(), quest_index);
                return 0;
            }

            pixel_position pos;

            if (!get_sectree_manager().map_base_position_by_map_index(ch->map_index(), pos)) {
                metin2::log::error("No base position for map {:d}", ch->map_index());
                return 0;
            }

            int x = pos.x + (int)lua_tonumber(lua, 2) * 100;
            int y = pos.y + (int)lua_tonumber(lua, 3) * 100;

            get_target_manager().create_target(ch->player_id(),
                quest_index,
                lua_tostring(lua, 1),
                TARGET_TYPE_POS,
                x,
                y,
                (int)lua_tonumber(lua, 4),
                lua_isstring(lua, 5) ? lua_tostring(lua, 5) : nullptr,
                lua_isnumber(lua, 6) ? (int)lua_tonumber(lua, 6) : 1);

            return 0;
        }

        auto target_vid(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            int quest_index = get_quest_manager().current_pc()->current_quest_index();

            if (!lua_isstring(lua, 1) || !lua_isnumber(lua, 2)) {
                metin2::log::error("Invalid arguments from player {} (quest index {:d})", ch->name(), quest_index);
                return 0;
            }

            get_target_manager().create_target(ch->player_id(),
                quest_index,
                lua_tostring(lua, 1),
                TARGET_TYPE_VID,
                (int)lua_tonumber(lua, 2),
                0,
                ch->map_index(),
                lua_isstring(lua, 3) ? lua_tostring(lua, 3) : nullptr,
                lua_isnumber(lua, 4) ? (int)lua_tonumber(lua, 4) : 1);

            return 0;
        }

        auto target_delete(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            int quest_index = get_quest_manager().current_pc()->current_quest_index();

            if (!lua_isstring(lua, 1)) {
                metin2::log::error("Invalid arguments from player {} (quest index {:d})", ch->name(), quest_index);
                return 0;
            }

            get_target_manager().delete_target(ch->player_id(), quest_index, lua_tostring(lua, 1));

            return 0;
        }

        auto target_clear(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            int quest_index = get_quest_manager().current_pc()->current_quest_index();

            get_target_manager().delete_target(ch->player_id(), quest_index, nullptr);

            return 0;
        }

        auto target_id(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            uint32_t quest_index = get_quest_manager().current_pc()->current_quest_index();

            if (!lua_isstring(lua, 1)) {
                metin2::log::error("Invalid arguments from player {} (quest index {:d})", ch->name(), quest_index);
                lua_pushnumber(lua, 0);
                return 1;
            }

            event_ptr event_value = get_target_manager().target_event(ch->player_id(), quest_index, (const char *)lua_tostring(lua, 1));

            if (event_value) {
                auto *info_value = dynamic_cast<target_info *>(event_value->info.get());

                if (info_value == nullptr) {
                    metin2::log::error("Target event has no info");
                    lua_pushnumber(lua, 0);
                    return 1;
                }

                if (info_value->type == TARGET_TYPE_VID) {
                    lua_pushnumber(lua, info_value->arg1);
                    return 1;
                }
            }

            lua_pushnumber(lua, 0);
            return 1;
        }

        void register_target_function_table() {
            luaL_reg target_functions[] =
                {
                    {"pos",    target_pos   },
                    {"vid",    target_vid   },
                    {"npc",    target_vid   },
                    {"pc",     target_vid   },
                    {"delete", target_delete},
                    {"clear",  target_clear },
                    {"id",     target_id    },
                    {nullptr,  nullptr      },
            };

            get_quest_manager().add_lua_function_table("target", target_functions);
        }
    };
}
