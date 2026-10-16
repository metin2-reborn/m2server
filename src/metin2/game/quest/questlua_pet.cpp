
#include <metin2/game/app/typedef.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/pet_system.h>
#include <metin2/game/quest/lua_incl.h>
#include <metin2/game/quest/questlua.h>
#include <metin2/game/quest/questmanager.h>

#include <cstdint>

namespace metin2::game {

    namespace quest {

        auto pet_summon(lua_State *lua) -> int {
            character *ch = quest_manager::instance().current_character_ptr();
            pet_system *pet_system = ch->get_pet_system();
            item *item = quest_manager::instance().current_item();
            if (!ch || !pet_system || !item) {
                lua_pushnumber(lua, 0);
                return 1;
            }

            if (nullptr == pet_system) {
                lua_pushnumber(lua, 0);
                return 1;
            }

            uint32_t mob_vnum = lua_isnumber(lua, 1) ? lua_tonumber(lua, 1) : 0;

            const char *pet_name = lua_isstring(lua, 2) ? lua_tostring(lua, 2) : nullptr;

            bool from_far = lua_isboolean(lua, 3) ? lua_toboolean(lua, 3) : false;

            pet_actor *pet = pet_system->summon(mob_vnum, item, pet_name, from_far);

            if (pet != nullptr)
                lua_pushnumber(lua, pet->get_vid());
            else
                lua_pushnumber(lua, 0);

            return 1;
        }

        auto pet_unsummon(lua_State *lua) -> int {
            character *ch = quest_manager::instance().current_character_ptr();
            pet_system *pet_system = ch->get_pet_system();

            if (nullptr == pet_system)
                return 0;

            uint32_t mob_vnum = lua_isnumber(lua, 1) ? lua_tonumber(lua, 1) : 0;

            pet_system->unsummon(mob_vnum);
            return 1;
        }

        auto pet_count_summoned(lua_State *lua) -> int {
            character *ch = quest_manager::instance().current_character_ptr();
            pet_system *pet_system = ch->get_pet_system();

            lua_Number count = 0;

            if (nullptr != pet_system)
                count = (lua_Number)pet_system->count_summoned();

            lua_pushnumber(lua, count);

            return 1;
        }

        auto pet_is_summon(lua_State *lua) -> int {
            character *ch = quest_manager::instance().current_character_ptr();
            pet_system *pet_system = ch->get_pet_system();

            if (nullptr == pet_system)
                return 0;

            uint32_t mob_vnum = lua_isnumber(lua, 1) ? lua_tonumber(lua, 1) : 0;

            pet_actor *pet_actor = pet_system->by_vnum(mob_vnum);

            if (nullptr == pet_actor)
                lua_pushboolean(lua, false);
            else
                lua_pushboolean(lua, pet_actor->is_summoned());

            return 1;
        }

        auto pet_spawn_effect(lua_State *lua) -> int {
            character *ch = quest_manager::instance().current_character_ptr();
            pet_system *pet_system = ch->get_pet_system();

            if (nullptr == pet_system)
                return 0;

            uint32_t mob_vnum = lua_isnumber(lua, 1) ? lua_tonumber(lua, 1) : 0;

            pet_actor *pet_actor = pet_system->by_vnum(mob_vnum);
            if (nullptr == pet_actor)
                return 0;
            character *pet_ch = pet_actor->get_character();
            if (nullptr == pet_ch)
                return 0;

            if (lua_isstring(lua, 2)) {
                pet_ch->specific_effect_packet(lua_tostring(lua, 2));
            }
            return 0;
        }

        void register_pet_function_table() {
            luaL_reg pet_functions[] =
                {
                    {"summon",         pet_summon        },
                    {"unsummon",       pet_unsummon      },
                    {"is_summon",      pet_is_summon     },
                    {"count_summoned", pet_count_summoned},
                    {"spawn_effect",   pet_spawn_effect  },
                    {nullptr,          nullptr           }
            };

            quest_manager::instance().add_lua_function_table("pet", pet_functions);
        }

    }
}
