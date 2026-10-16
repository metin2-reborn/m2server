
#include <metin2/game/app/typedef.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/quest/lua_incl.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/world/entity.h>
#include <metin2/game/world/sectree_manager.h>

namespace metin2::game {

    namespace quest {
        auto dance_event_go_home(lua_State *lua) -> int {
            sectree_map *sec_map = sectree_manager::instance().map(115);

            if (sec_map != nullptr) {
                sec_map->for_each([](entity *ent) {
                    if (ent->is_type(ENTITY_CHARACTER)) {
                        auto *ch = (character *)ent;

                        if (ch->is_pc() == true && ch->is_gm() != true) {
                            if (((ch->x() >= 764503 && ch->x() <= 772362) && (ch->y() >= 22807 && ch->y() <= 26499)) == false) {
                                ch->go_home();
                            }
                        }
                    }
                });
            }

            return 0;
        }

        void register_dance_event_function_table() {
            luaL_reg dance_event_functions[] =
                {
                    {"gohome", dance_event_go_home},

                    {nullptr,  nullptr            }
            };

            quest_manager::instance().add_lua_function_table("dance_event", dance_event_functions);
        }
    }
}
