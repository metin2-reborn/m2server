

#include <metin2/game/characters/character.h>
#include <metin2/game/events/dragon_lair.h>
#include <metin2/game/guilds/guild.h>
#include <metin2/game/quest/lua_incl.h>
#include <metin2/game/quest/questmanager.h>

#include <cstdint>

namespace metin2::game {

    namespace quest {
        auto dl_start_raid(lua_State *lua) -> int {
            character *ch = quest_manager::instance().current_character_ptr();
            int32_t baseMapIndex = lua_tonumber(lua, -1);

            dragon_lair_manager::instance().start(ch->map_index(), baseMapIndex, ch->get_guild()->id());

            return 0;
        }

        void register_dragon_lair_function_table() {
            luaL_reg dl_functions[] =
                {
                    {"startRaid", dl_start_raid},

                    {nullptr,     nullptr      }
            };

            quest_manager::instance().add_lua_function_table("DragonLair", dl_functions);
        }
    }
}
