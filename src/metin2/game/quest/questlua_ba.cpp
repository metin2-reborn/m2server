
#include <metin2/game/events/battle_arena.h>
#include <metin2/game/quest/lua_incl.h>
#include <metin2/game/quest/questmanager.h>

namespace metin2::game {

    namespace quest {
        auto ba_start(lua_State *lua) -> int {
            if (lua_isnumber(lua, 1)) {
                battle_arena::instance().start((int)lua_tonumber(lua, 1));
            }

            return 0;
        }

        void register_battle_arena_function_table() {
            luaL_reg ba_functions[] =
                {
                    {"start", ba_start},

                    {nullptr, nullptr }
            };

            quest_manager::instance().add_lua_function_table("ba", ba_functions);
        }
    }
}
