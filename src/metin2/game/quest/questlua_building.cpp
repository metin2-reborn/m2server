#include <metin2/game/app/config.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/guilds/building.h>
#include <metin2/game/guilds/guild.h>
#include <metin2/game/persistence/db.h>
#include <metin2/game/quest/lua_incl.h>
#include <metin2/game/quest/questmanager.h>

#include <metin2/common/utils.h>
#include <metin2/log/log.h>
#include <metin2/proto/building.h>
#include <metin2/sql/query.h>

#include <mysql.h>

#include <cstdint>
#include <memory>

namespace metin2::game {

    namespace quest {
        auto building_get_land_id(lua_State *lua) -> int {
            using namespace building;

            if (!lua_isnumber(lua, 1) || !lua_isnumber(lua, 2) || !lua_isnumber(lua, 3)) {
                metin2::log::error("Invalid arguments");
                lua_pushnumber(lua, 0);
                return 1;
            }

            land *pkLand = manager::instance().find_land((int)lua_tonumber(lua, 1), (int)lua_tonumber(lua, 2), (int)lua_tonumber(lua, 3));
            lua_pushnumber(lua, pkLand ? pkLand->id() : 0);
            return 1;
        }

        auto building_get_land_info(lua_State *lua) -> int {
            int price = 1000000000;
            int owner = 1000000000;
            int level_limit = 1000000000;

            if (lua_isnumber(lua, 1)) {
                using namespace building;

                land *pkLand = manager::instance().find_land((uint32_t)lua_tonumber(lua, 1));

                if (pkLand) {
                    const land_table &t = pkLand->data();

                    price = t.price;
                    owner = t.guild_id;
                    level_limit = t.guild_level_limit;
                }
            } else
                metin2::log::error("Invalid land id argument");

            lua_pushnumber(lua, price);
            lua_pushnumber(lua, owner);
            lua_pushnumber(lua, level_limit);
            return 3;
        }

        auto building_set_land_owner(lua_State *lua) -> int {
            if (!lua_isnumber(lua, 1) || !lua_isnumber(lua, 2)) {
                metin2::log::error("Invalid arguments");
                return 0;
            }

            using namespace building;

            land *pkLand = manager::instance().find_land((uint32_t)lua_tonumber(lua, 1));

            if (pkLand) {
                if (pkLand->data().guild_id == 0)
                    pkLand->set_owner((uint32_t)lua_tonumber(lua, 2));
            }

            return 0;
        }

        auto building_has_land(lua_State *lua) -> int {
            using namespace building;

            if (!lua_isnumber(lua, 1)) {
                metin2::log::error("Invalid guild id argument");
                lua_pushboolean(lua, true);
                return 1;
            }

            auto pmsg = db_manager::instance().direct_query(sql::format("SELECT COUNT(*) FROM land{} WHERE guild_id = {}", sql::raw(get_table_postfix()), (uint32_t)lua_tonumber(lua, 1)));

            if (pmsg->get()->num_rows > 0) {
                MYSQL_ROW row = mysql_fetch_row(pmsg->get()->res.get());

                int count = 0;
                str_to_number(count, row[0]);

                if (count == 0) {
                    lua_pushboolean(lua, false);
                } else {
                    lua_pushboolean(lua, true);
                }
            } else {
                lua_pushboolean(lua, true);
            }

            return 1;
        }

        auto building_reconstruct(lua_State *lua) -> int {
            using namespace building;

            uint32_t dwNewBuilding = (uint32_t)lua_tonumber(lua, 1);

            quest_manager &q = quest_manager::instance();

            character *npc = q.current_npc_character_ptr();
            if (!npc)
                return 0;

            guild *pGuild = npc->get_guild();
            if (!pGuild)
                return 0;

            land *pLand = manager::instance().find_land_by_guild(pGuild->id());
            if (!pLand)
                return 0;

            object *pObject = pLand->find_object_by_npc(npc);
            if (!pObject)
                return 0;

            pObject->reconstruct(dwNewBuilding);

            return 0;
        }

        void register_building_function_table() {
            luaL_reg functions[] =
                {
                    {"get_land_id",    building_get_land_id   },
                    {"get_land_info",  building_get_land_info },
                    {"set_land_owner", building_set_land_owner},
                    {"has_land",       building_has_land      },
                    {"reconstruct",    building_reconstruct   },
                    {nullptr,          nullptr                }
            };

            quest_manager::instance().add_lua_function_table("building", functions);
        }
    };
}
