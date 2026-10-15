
#include <metin2/game/app/config.h>
#include <metin2/game/app/event.h>
#include <metin2/game/text/locale_service.h>
#include <metin2/game/quest/lua_incl.h>
#include <metin2/game/events/ox_event.h>
#include <metin2/game/quest/questmanager.h>

#include <cstdio>

namespace metin2::game {

namespace quest
{
	auto oxevent_get_status(lua_State* lua) -> int
	{
		ox_event_status ret = ox_event_manager::instance().status();

		lua_pushnumber(lua, (int)ret);

		return 1;
	}

	auto oxevent_open(lua_State* lua) -> int
	{
		ox_event_manager::instance().clear_quiz();

		char script[256];
		snprintf(script, sizeof(script), "%s/oxquiz.lua", locale_service_get_base_path().c_str());
		int result = lua_dofile(quest::quest_manager::instance().lua_state(), script);

		if (result != 0)
		{
			lua_pushnumber(lua, 0);
			return 1;
		}
		else
		{
			lua_pushnumber(lua, 1);
		}
		
		ox_event_manager::instance().set_status(OXEVENT_OPEN);

		return 1;
	}
	
	auto oxevent_close(lua_State* lua) -> int
	{
		ox_event_manager::instance().set_status(OXEVENT_CLOSE);
		
		return 0;
	}
	
	auto oxevent_quiz(lua_State* lua) -> int
	{
		if (lua_isnumber(lua, 1) && lua_isnumber(lua, 2))
		{
			bool ret = ox_event_manager::instance().quiz((int)lua_tonumber(lua, 1), (int)lua_tonumber(lua, 2));

			if (ret == false)
			{
				lua_pushnumber(lua, 0);
			}
			else
			{
				lua_pushnumber(lua, 1);
			}
		}

		return 1;
	}
	
	auto oxevent_get_attender(lua_State* lua) -> int
	{
		lua_pushnumber(lua, (int)ox_event_manager::instance().attender_count());
		return 1;
	}

	EVENTINFO(end_oxevent_info)
	{
		int empty;

		end_oxevent_info() 
		: empty( 0 )
		{
		}
	};

	EVENTFUNC(end_oxevent)
	{
		ox_event_manager::instance().close_event();
		return 0;
	}

	auto oxevent_end_event(lua_State* lua) -> int
	{
		ox_event_manager::instance().set_status(OXEVENT_FINISH);

		end_oxevent_info* info = alloc_event_info<end_oxevent_info>();
		event_create(end_oxevent, info, passes_per_sec(5));

		return 0;
	}

	auto oxevent_end_event_force(lua_State* lua) -> int
	{
		ox_event_manager::instance().close_event();
		ox_event_manager::instance().set_status(OXEVENT_FINISH);

		return 0;
	}

	auto oxevent_give_item(lua_State* lua) -> int
	{
		if (lua_isnumber(lua, 1) && lua_isnumber(lua, 2))
		{
			ox_event_manager::instance().give_item_to_attender((int)lua_tonumber(lua, 1), (int)lua_tonumber(lua, 2));
		}

		return 0;
	}
	
	void register_ox_event_function_table()
	{
		luaL_reg oxevent_functions[] = 
		{
			{	"get_status",	oxevent_get_status	},
			{	"open",			oxevent_open		},
			{	"close",		oxevent_close		},
			{	"quiz",			oxevent_quiz		},
			{	"get_attender",	oxevent_get_attender},
			{	"end_event",	oxevent_end_event	},
			{	"end_event_force",	oxevent_end_event_force	},
			{	"give_item",	oxevent_give_item	},

			{ nullptr, nullptr}
		};

		quest_manager::instance().add_lua_function_table("oxevent", oxevent_functions);
	}
}
}
