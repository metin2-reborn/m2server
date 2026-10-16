
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
            pet_system *petSystem = ch->get_pet_system();
            item *pItem = quest_manager::instance().current_item();
            if (!ch || !petSystem || !pItem) {
                lua_pushnumber(lua, 0);
                return 1;
            }

            if (nullptr == petSystem) {
                lua_pushnumber(lua, 0);
                return 1;
            }

            uint32_t mobVnum = lua_isnumber(lua, 1) ? lua_tonumber(lua, 1) : 0;

            const char *petName = lua_isstring(lua, 2) ? lua_tostring(lua, 2) : nullptr;

            bool bFromFar = lua_isboolean(lua, 3) ? lua_toboolean(lua, 3) : false;

            pet_actor *pet = petSystem->summon(mobVnum, pItem, petName, bFromFar);

            if (pet != nullptr)
                lua_pushnumber(lua, pet->get_vid());
            else
                lua_pushnumber(lua, 0);

            return 1;
        }

        auto pet_unsummon(lua_State *lua) -> int {
            character *ch = quest_manager::instance().current_character_ptr();
            pet_system *petSystem = ch->get_pet_system();

            if (nullptr == petSystem)
                return 0;

            uint32_t mobVnum = lua_isnumber(lua, 1) ? lua_tonumber(lua, 1) : 0;

            petSystem->unsummon(mobVnum);
            return 1;
        }

        auto pet_count_summoned(lua_State *lua) -> int {
            character *ch = quest_manager::instance().current_character_ptr();
            pet_system *petSystem = ch->get_pet_system();

            lua_Number count = 0;

            if (nullptr != petSystem)
                count = (lua_Number)petSystem->count_summoned();

            lua_pushnumber(lua, count);

            return 1;
        }

        auto pet_is_summon(lua_State *lua) -> int {
            character *ch = quest_manager::instance().current_character_ptr();
            pet_system *petSystem = ch->get_pet_system();

            if (nullptr == petSystem)
                return 0;

            uint32_t mobVnum = lua_isnumber(lua, 1) ? lua_tonumber(lua, 1) : 0;

            pet_actor *petActor = petSystem->by_vnum(mobVnum);

            if (nullptr == petActor)
                lua_pushboolean(lua, false);
            else
                lua_pushboolean(lua, petActor->is_summoned());

            return 1;
        }

        auto pet_spawn_effect(lua_State *lua) -> int {
            character *ch = quest_manager::instance().current_character_ptr();
            pet_system *petSystem = ch->get_pet_system();

            if (nullptr == petSystem)
                return 0;

            uint32_t mobVnum = lua_isnumber(lua, 1) ? lua_tonumber(lua, 1) : 0;

            pet_actor *petActor = petSystem->by_vnum(mobVnum);
            if (nullptr == petActor)
                return 0;
            character *pet_ch = petActor->get_character();
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
