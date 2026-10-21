
#include <metin2/game/events/battle_arena.h>
#include <metin2/game/quest/lua_incl.h>
#include <metin2/game/quest/questmanager.h>

namespace metin2::game {

    namespace quest {
        auto ba_start(lua_State *lua) -> int {
            if (lua_isnumber(lua, 1)) {
                get_battle_arena().start((int)lua_tonumber(lua, 1));
            }

            return 0;
        }

        void register_battle_arena_function_table() {
            luaL_reg ba_functions[] =
                {
                    {"start", ba_start},

                    {nullptr, nullptr }
            };

            get_quest_manager().add_lua_function_table("ba", ba_functions);
        }
    }
}
