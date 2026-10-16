
#include <metin2/game/events/blue_dragon_binder.h>

#include <metin2/game/quest/lua_incl.h>
#include <metin2/game/quest/questmanager.h>

#include <metin2/log/log.h>

#include <cstdarg>
#include <cstddef>

namespace metin2::game::inline events {

auto blue_dragon_get_skill_factor(const size_t cnt, ...) -> unsigned int
{
	lua_State* L = quest::quest_manager::instance().lua_state();

	const int stack_top = lua_gettop(L);

	lua_getglobal( L, "BlueDragonSetting" );

	if (false == lua_istable(L, -1))
	{
		lua_settop( L, stack_top );

		return 0;
	}

	va_list vl;

	va_start(vl, cnt);

	for( size_t i=0 ; i < cnt ; ++i )
	{
		const char* key = va_arg(vl, const char*);

		if (nullptr == key)
		{
			va_end(vl);
			lua_settop( L, stack_top );
			metin2::log::error("Blue dragon: invalid key list in skill config");
			return 0;
		}

		lua_pushstring( L, key );
		lua_gettable( L, -2 );

		if (false == lua_istable(L, -1) && i != cnt-1)
		{
			va_end(vl);
			lua_settop( L, stack_top );
			metin2::log::error("Blue dragon: key {} is not a table in skill config", key);
			return 0;
		}
	}

	va_end(vl);

	if (false == lua_isnumber(L, -1))
	{
		lua_settop( L, stack_top );
		metin2::log::error("Blue dragon: last key in skill config is not a number");
		return 0;
	}

	int val = static_cast<int>(lua_tonumber( L, -1 ));

	lua_settop( L, stack_top );

	return val;
}

auto blue_dragon_get_range_factor(const char* key, const int val) -> unsigned int
{
	lua_State* L = quest::quest_manager::instance().lua_state();

	const int stack_top = lua_gettop(L);

	lua_getglobal( L, "BlueDragonSetting" );

	if (false == lua_istable(L, -1))
	{
		lua_settop( L, stack_top );

		return 0;
	}

	lua_pushstring( L, key );
	lua_gettable( L, -2 );

	if (false == lua_istable(L, -1))
	{
		lua_settop( L, stack_top );

		metin2::log::error("Blue dragon: required table {} missing from skill config", key);
		return 0;
	}

	const size_t cnt = static_cast<size_t>(luaL_getn(L, -1));

	for( size_t i=1 ; i <= cnt ; ++i )
	{
		lua_rawgeti( L, -1, i );

		if (false == lua_istable(L, -1))
		{
			lua_settop( L, stack_top );

			metin2::log::error("Blue dragon: invalid index {1:d} in skill config table {0}", key, i);
			return 0;
		}

		lua_pushstring( L, "min" );
		lua_gettable( L, -2 );

		if (false == lua_isnumber(L, -1))
		{
			lua_settop( L, stack_top );

			metin2::log::error("Blue dragon: no min value in skill config table {}", key);
			return 0;
		}

		const int min = static_cast<int>(lua_tonumber(L, -1));

		lua_pop(L, 1);

		lua_pushstring( L, "max" );
		lua_gettable( L, -2 );

		if (false == lua_isnumber(L, -1))
		{
			lua_settop( L, stack_top );

			metin2::log::error("Blue dragon: no max value in skill config table {}", key);
			return 0;
		}

		const int max = static_cast<int>(lua_tonumber(L, -1));

		lua_pop(L, 1);

		if (min <= val && val <= max)
		{
			lua_pushstring( L, "pct" );
			lua_gettable( L, -2 );

			if (false == lua_isnumber(L, -1))
			{
				lua_settop( L, stack_top );

				metin2::log::error("Blue dragon: no pct value in skill config table {}", key);
				return 0;
			}

			const int pct = static_cast<int>(lua_tonumber(L, -1));

			lua_settop( L, stack_top );

			return pct;
		}

		lua_pop(L, 1);
	}

	lua_settop( L, stack_top );

	return 0;
}

auto blue_dragon_get_index_factor(const char* container, const size_t idx, const char* key) -> unsigned int
{
	lua_State* L = quest::quest_manager::instance().lua_state();

	const int stack_top = lua_gettop(L);

	lua_getglobal( L, "BlueDragonSetting" );

	if (false == lua_istable(L, -1))
	{
		lua_settop( L, stack_top );

		return 0;
	}

	lua_pushstring( L, container );
	lua_gettable( L, -2 );

	if (false == lua_istable(L, -1))
	{
		lua_settop( L, stack_top );

		metin2::log::error("Blue dragon: required table {} missing from skill config", key);
		return 0;
	}

	lua_rawgeti( L, -1, idx );

	if (false == lua_istable(L, -1))
	{
		lua_settop( L, stack_top );

		metin2::log::error("Blue dragon: invalid index {1:d} in skill config table {0}", key, idx);
		return 0;
	}

	lua_pushstring( L, key );
	lua_gettable( L, -2 );

	if (false == lua_isnumber(L, -1))
	{
		lua_settop( L, stack_top );

		metin2::log::error("Blue dragon: no min value in skill config table {}", key);
		return 0;
	}

	const unsigned int ret = static_cast<unsigned int>(lua_tonumber(L, -1));

	lua_settop( L, stack_top );

	return ret;
}
}
