#include <metin2/game/quest/lua_incl.h>
#include <metin2/game/quest/questlua.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/events/speed_server.h>

#include <metin2/log/log.h>

#include <cstdint>
#include <list>

namespace metin2::game {

namespace quest
{

	int speedserver_get_wday (lua_State* lua)
	{
		if (!lua_isnumber(lua,1) || !lua_isnumber(lua,2))
		{
			metin2::log::error("wrong argument");
		}

		uint8_t empire = lua_tonumber(lua,1);

		if (empire > 3)
		{
			metin2::log::error("invalid empire");
			return 0;
		}

		int wday = lua_tonumber(lua,2) - 1;

		if (wday < 0 || wday > 6)
		{
			metin2::log::error("wrong day");
			return 0;
		}

		metin2::log::info( "empire {:d} wday {:d}",empire, wday);

		std::list <hme> time_lst = speed_server_manager::instance().wday_exp_table_of_empire(empire, wday);

		int i = 0;
		for (std::list <hme>::iterator it = time_lst.begin();
				it != time_lst.end(); it++)
		{
			metin2::log::info( "{:d}",i);
			lua_pushnumber (lua, it->hour);
			lua_pushnumber (lua, it->min);
			lua_pushnumber (lua, it->exp);
			i++;
		}

		return i * 3;
	}

	int speedserver_set_wday (lua_State* lua)
	{
		uint8_t empire = lua_tonumber (lua, 1);
		int wday = lua_tonumber (lua, 2);
		uint8_t end_hour = lua_tonumber(lua, 3);
		uint8_t end_minite = lua_tonumber(lua, 4);
		int exp_percent = lua_tonumber(lua, 5);

		speed_server_manager::instance().set_wday_exp_table_of_empire (empire, wday - 1, hme (end_hour, end_minite, exp_percent));

		return 0;
	}

	int speedserver_init_wday (lua_State* lua)
	{
		if (!lua_isnumber (lua, 1) || !lua_isnumber (lua, 2))
		{
			metin2::log::error("invalid argument.");
			return 0;
		}

		uint8_t empire = lua_tonumber (lua, 1);
		int wday = lua_tonumber (lua, 2);
		
		metin2::log::info( "init_wday {:d} {:d}",empire, wday);

		speed_server_manager::instance().init_wday_exp_table_of_empire (empire, wday - 1);
		
		return 0;
	}

	int speedserver_get_holiday (lua_State* lua)
	{
		if (!lua_isnumber(lua,1) || !lua_isnumber(lua,2) || !lua_isnumber(lua,3) || !lua_isnumber(lua,4))
		{
			metin2::log::error("wrong argument");
		}

		uint8_t empire = lua_tonumber(lua,1);

		if (empire > 4)
		{
			metin2::log::error("invalid empire");
			return 0;
		}

		date date = game::events::date (lua_tonumber(lua,2) - 1900, lua_tonumber(lua,3) - 1, lua_tonumber(lua,4));
		
		metin2::log::info( "empire {:d} date {:d} {:d} {:d}", empire, date.year, date.mon, date.day);
		
		bool is_exist;

		std::list <hme> time_lst = speed_server_manager::instance().holiday_exp_table_of_empire(empire, date, is_exist);

		int i = 0;
		if (is_exist)
		{
			for (std::list <hme>::iterator it = time_lst.begin();
					it != time_lst.end(); it++)
			{
				lua_pushnumber (lua, it->hour);
				lua_pushnumber (lua, it->min);
				lua_pushnumber (lua, it->exp);
				i++;
			}
			return i * 3;
		}
		else
			return 0;

	}

	int speedserver_set_holiday (lua_State* lua)
	{
		if (!lua_isnumber(lua,1) || !lua_isnumber(lua,2) || !lua_isnumber(lua,3) || !lua_isnumber(lua,4) 
				|| !lua_isnumber(lua,5) || !lua_isnumber(lua,6) || !lua_isnumber(lua,7))
		{
			metin2::log::error("wrong argument");
		}

		uint8_t empire = lua_tonumber (lua, 1);
		date date = game::events::date (lua_tonumber(lua,2) - 1900, lua_tonumber(lua,3) - 1, lua_tonumber(lua,4));
		uint8_t end_hour = lua_tonumber(lua, 5);
		uint8_t end_minite = lua_tonumber(lua, 6);
		int exp_percent = lua_tonumber(lua, 7);

		metin2::log::info("h {:d} m {:d} e {:d}", end_hour, end_minite, exp_percent);

		speed_server_manager::instance().set_holiday_exp_table_of_empire (empire, date, hme (end_hour, end_minite, exp_percent));

		return 0;
	}

	int speedserver_init_holiday (lua_State* lua)
	{
		if (!lua_isnumber(lua,1) || !lua_isnumber(lua,2) || !lua_isnumber(lua,3) || !lua_isnumber(lua,4))
		{
			metin2::log::error("wrong argument");
		}

		uint8_t empire = lua_tonumber (lua, 1);
		date date = game::events::date (lua_tonumber(lua,2) - 1900, lua_tonumber(lua,3) - 1, lua_tonumber(lua,4));
	
		speed_server_manager::instance().init_holiday_exp_table_of_empire (empire, date);
		
		return 0;
	}

	int speedserver_get_current_exp_priv (lua_State* lua)
	{
		if (!lua_isnumber (lua, 1))
		{
			metin2::log::error("invalid empire");
			return 0;
		}
		uint8_t empire = lua_tonumber(lua, 1);

		int duration;
		bool is_change;

		hme hme = speed_server_manager::instance().current_exp_priv_of_empire (empire, duration, is_change);

		lua_pushnumber (lua, hme.hour);
		lua_pushnumber (lua, hme.min);
		lua_pushnumber (lua, hme.exp);
		lua_pushnumber (lua, duration);
		lua_pushboolean (lua, is_change);

		metin2::log::info( "empire : {:d} is_change : {:d}",empire, is_change);

		return 5;
	}

	void register_speed_server_function_table()
	{
		luaL_reg speed_server_functions[] = 
		{
			{	"get_holiday",	speedserver_get_holiday	},
			{	"set_holiday",	speedserver_set_holiday	},
			{	"get_wday",	speedserver_get_wday	},
			{	"set_wday",	speedserver_set_wday	},
			{	"init_holiday", speedserver_init_holiday	},
			{	"init_wday", speedserver_init_wday	},
			{	"get_current_exp_priv", speedserver_get_current_exp_priv	},

			{ nullptr, nullptr}
		};

		quest_manager::instance().add_lua_function_table("speedserver", speed_server_functions);
	}
}
}
