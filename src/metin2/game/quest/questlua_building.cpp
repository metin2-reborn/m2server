#include <metin2/game/guilds/building.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/app/config.h>
#include <metin2/game/persistence/db.h>
#include <metin2/game/guilds/guild.h>
#include <metin2/game/quest/lua_incl.h>
#include <metin2/game/quest/questmanager.h>

#include <metin2/proto/building.h>
#include <metin2/common/utils.h>
#include <metin2/log/log.h>

#include <mysql.h>

#include <cstdint>
#include <memory>

namespace metin2::game {

namespace quest
{
	//
	// "building" Lua functions
	//
	int building_get_land_id(lua_State * L)
	{
		using namespace building;

		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3))
		{
			metin2::log::error("invalid argument");
			lua_pushnumber(L, 0);
			return 1;
		}

		land * pkLand = manager::instance().find_land((int) lua_tonumber(L, 1), (int) lua_tonumber(L, 2), (int) lua_tonumber(L, 3));
		lua_pushnumber(L, pkLand ? pkLand->id() : 0);
		return 1;
	}

	int building_get_land_info(lua_State * L)
	{
		int price = 1000000000;
		int owner = 1000000000;
		int level_limit = 1000000000;

		if (lua_isnumber(L, 1))
		{
			using namespace building;

			land * pkLand = manager::instance().find_land((uint32_t) lua_tonumber(L, 1));

			if (pkLand)
			{
				const land_table & t = pkLand->data();

				price = t.price;
				owner = t.guild_id;
				level_limit = t.guild_level_limit;
			}
		}
		else
			metin2::log::error("invalid argument");

		lua_pushnumber(L, price);
		lua_pushnumber(L, owner);
		lua_pushnumber(L, level_limit);
		return 3;
	}

	int building_set_land_owner(lua_State * L)
	{
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2))
		{
			metin2::log::error("invalid argument");
			return 0;
		}

		using namespace building;

		land * pkLand = manager::instance().find_land((uint32_t) lua_tonumber(L, 1));

		if (pkLand)
		{
			if (pkLand->data().guild_id == 0)
				pkLand->set_owner((uint32_t) lua_tonumber(L, 2));
		}

		return 0;
	}

	int building_has_land(lua_State * L)
	{
		using namespace building;

		if (!lua_isnumber(L, 1))
		{
			metin2::log::error("invalid argument");
			lua_pushboolean(L, true);
			return 1;
		}

		std::unique_ptr<ql_msg> pmsg(db_manager::instance().direct_query("SELECT COUNT(*) FROM land%s WHERE guild_id = %d", get_table_postfix(), (uint32_t)lua_tonumber(L,1)));

		if ( pmsg->get()->num_rows > 0 )
		{
			MYSQL_ROW row = mysql_fetch_row(pmsg->get()->sql_result);

			int	count = 0;
			str_to_number(count, row[0]);

			if (count == 0)
			{
				lua_pushboolean(L, false);
			}
			else
			{
				lua_pushboolean(L, true);
			}
		}
		else
		{
			lua_pushboolean(L, true);
		}

		return 1;
	}

	int building_reconstruct(lua_State* L)
	{
		using namespace building;

		uint32_t dwNewBuilding = (uint32_t)lua_tonumber(L, 1);

		quest_manager& q = quest_manager::instance();

		character * npc = q.current_npc_character_ptr();
		if (!npc)
			return 0;

		guild* pGuild = npc->get_guild();
		if (!pGuild)
			return 0;

		land* pLand = manager::instance().find_land_by_guild(pGuild->id());
		if (!pLand)
			return 0;

		object * pObject = pLand->find_object_by_npc(npc);
		if (!pObject)
			return 0;

		pObject->reconstruct(dwNewBuilding);

		return 0;
	}

	void register_building_function_table()
	{
		luaL_reg functions[] =
		{
			{ "get_land_id",	building_get_land_id	},
			{ "get_land_info",	building_get_land_info	},
			{ "set_land_owner",	building_set_land_owner	},
			{ "has_land",	building_has_land	},
			{ "reconstruct",	building_reconstruct	},
			{ nullptr,		nullptr			}
		};

		quest_manager::instance().add_lua_function_table("building", functions);
	}
};
}
