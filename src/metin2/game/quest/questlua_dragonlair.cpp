

#include <metin2/game/characters/character.h>
#include <metin2/game/events/dragon_lair.h>
#include <metin2/game/guilds/guild.h>
#include <metin2/game/quest/lua_incl.h>
#include <metin2/game/quest/questmanager.h>

#include <cstdint>

namespace metin2::game {

    namespace quest {
        auto dl_start_raid(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            int32_t base_map_index = lua_tonumber(lua, -1);

            get_dragon_lair_manager().start(ch->map_index(), base_map_index, ch->get_guild()->id());

            return 0;
        }

        void register_dragon_lair_function_table() {
            luaL_reg dl_functions[] =
                {
                    {"startRaid", dl_start_raid},

                    {nullptr,     nullptr      }
            };

            get_quest_manager().add_lua_function_table("DragonLair", dl_functions);
        }
    }
}
