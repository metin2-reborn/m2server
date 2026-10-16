
#include <metin2/game/events/arena.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/quest/lua_incl.h>
#include <metin2/game/quest/questmanager.h>

#include <metin2/log/log.h>

#include <cstdint>

namespace metin2::game {

namespace quest
{
	auto arena_start_duel(lua_State * lua) -> int
	{
		character * ch = quest_manager::instance().current_character_ptr();
		character * ch2 = character_manager::instance().find_pc(lua_tostring(lua,1));
		int nSetPoint = (int)lua_tonumber(lua, 2);

		if ( ch == nullptr || ch2 == nullptr )
		{
			lua_pushnumber(lua, 0);
			return 1;
		}

		if ( ch->is_horse_riding() == true )
		{
			ch->stop_riding();
			ch->horse_summon(false);
		}

		if ( ch2->is_horse_riding() == true )
		{
			ch2->stop_riding();
			ch2->horse_summon(false);
		}

		if ( arena_manager::instance().is_member(ch->map_index(), ch->player_id()) != MEMBER_NO || 
				arena_manager::instance().is_member(ch2->map_index(), ch2->player_id()) != MEMBER_NO	)
		{
			lua_pushnumber(lua, 2);
			return 1;
		}
		if ( arena_manager::instance().start_duel(ch, ch2, nSetPoint) == false )
		{
			lua_pushnumber(lua, 3);
			return 1;
		}

		lua_pushnumber(lua, 1);

		return 1;
	}

	auto arena_add_map(lua_State* lua) -> int
	{
		int mapIdx		= (int)lua_tonumber(lua, 1);
		int startposAX	= (int)lua_tonumber(lua, 2);
		int startposAY	= (int)lua_tonumber(lua, 3);
		int startposBX	= (int)lua_tonumber(lua, 4);
		int startposBY	= (int)lua_tonumber(lua, 5);

		if ( arena_manager::instance().add_arena(mapIdx, startposAX, startposAY, startposBX, startposBY) == false )
		{
			metin2::log::error("Cannot add arena on map {:d} (start A {:d},{:d}, start B {:d},{:d})", mapIdx, startposAX, startposAY, startposBX, startposBY);
		}
		else
		{
			metin2::log::info("Arena: added map {:d} (start A {:d},{:d}, start B {:d},{:d})", mapIdx, startposAX, startposAY, startposBX, startposBY);
		}

		return 1;
	}

	auto arena_get_duel_list(lua_State* lua) -> int
	{
		arena_manager::instance().duel_list(lua);

		return 1;
	}

	auto arena_add_observer(lua_State* lua) -> int
	{
		int mapIdx = (int)lua_tonumber(lua, 1);
		int ObPointX = (int)lua_tonumber(lua, 2);
		int ObPointY = (int)lua_tonumber(lua, 3);
		character * ch = quest_manager::instance().current_character_ptr();

		arena_manager::instance().add_observer(ch, mapIdx, ObPointX, ObPointY);

		return 1;
	}

	auto arena_is_in_arena(lua_State* lua) -> int
	{
		uint32_t pid = (uint32_t)lua_tonumber(lua, 1);

		character * ch = character_manager::instance().find_by_pid(pid);

		if ( ch == nullptr )
		{
			lua_pushnumber(lua, 1);
		}
		else
		{
			if ( ch->get_arena() == nullptr || ch->arena_observer_mode() == true )
			{
				if ( arena_manager::instance().is_member(ch->map_index(), ch->player_id()) == MEMBER_DUELIST )
					lua_pushnumber(lua, 1);
				else
					lua_pushnumber(lua, 0);
			}
			else
			{
				lua_pushnumber(lua, 0);
			}
		}
		return 1;
	}

	void register_arena_function_table()
	{
		luaL_reg arena_functions[] =
		{
			{"start_duel",		arena_start_duel		},
			{"add_map",			arena_add_map			},
			{"get_duel_list",	arena_get_duel_list		},
			{"add_observer",	arena_add_observer		},
			{"is_in_arena",		arena_is_in_arena		},

			{nullptr,	nullptr}
		};

		quest_manager::instance().add_lua_function_table("arena", arena_functions);
	}
}
}
