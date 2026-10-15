#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/quest/lua_incl.h>
#include <metin2/game/empire/monarch.h>
#include <metin2/game/quest/questmanager.h>

#include <metin2/proto/tables.h>

#include <cstdint>

namespace metin2::game {


namespace quest
{
	int mgmt_monarch_state(lua_State* lua)
	{
		if ( lua_isnumber(lua, 1) == false )
		{
			return 0;
		}

		int idx = (int)lua_tonumber(lua, 1);

		if ( idx < 1 || idx > 3 )
		{
			return 0;
		}

		monarch_info* info = monarch::instance().get_monarch();

		lua_pushstring(lua, info->name[idx]);
		lua_pushnumber(lua, info->pid[idx]);
		lua_pushstring(lua, info->date[idx]);
		lua_pushnumber(lua, info->money[idx]);

		return 4;
	}

	int mgmt_monarch_change_lord(lua_State* lua)
	{
		if ( lua_isnumber(lua, 1) == false || lua_isnumber(lua, 2) == false )
		{
			return 0;
		}

		packet_change_monarch_lord info;
		info.empire = (uint8_t)lua_tonumber(lua, 1);
		info.pid = (uint32_t)lua_tonumber(lua, 2);

		db_clientdesc->db_packet(HEADER_GD_CHANGE_MONARCH_LORD, 0, &info, sizeof(info));

		return 0;
	}
	
	void register_mgmt_function_table()
	{
		luaL_reg mgmt_functions[] =
		{
			{	"monarch_state",		mgmt_monarch_state	},
			{	"monarch_change_lord",	mgmt_monarch_change_lord	},

			{	nullptr,	nullptr}
		};
		
		quest_manager::instance().add_lua_function_table("mgmt", mgmt_functions);
	}
}
}
