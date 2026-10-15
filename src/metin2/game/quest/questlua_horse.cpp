
#include <metin2/game/characters/affect.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/app/config.h>
#include <metin2/game/characters/horse_rider.h>
#include <metin2/game/characters/horsename_manager.h>
#include <metin2/game/text/locale_service.h>
#include <metin2/game/quest/lua_incl.h>
#include <metin2/game/quest/questlua.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/app/utils.h>

#include <metin2/log/log.h>

#include <algorithm>
#include <cstdint>

namespace metin2::game {

namespace quest
{
	//
	// "horse" Lua functions
	//
	int horse_is_riding(lua_State* lua)
	{
		character * ch = quest_manager::instance().current_character_ptr();

		if (ch->is_horse_riding())
			lua_pushnumber(lua, 1);
		else
			lua_pushnumber(lua, 0);

		return 1;
	}

	int horse_is_summon(lua_State* lua)
	{
		character * ch = quest_manager::instance().current_character_ptr();

		if (nullptr != ch)
		{
			lua_pushboolean(lua, (ch->horse() != nullptr) ? true : false);
		}
		else
		{
			lua_pushboolean(lua, false);
		}

		return 1;
	}

	int horse_ride(lua_State* lua)
	{
		character * ch = quest_manager::instance().current_character_ptr();
		ch->start_riding();
		return 0;
	}

	int horse_unride(lua_State* lua)
	{
		character * ch = quest_manager::instance().current_character_ptr();
		ch->stop_riding();
		return 0;
	}

	int horse_summon(lua_State* lua)
	{
		character * ch = quest_manager::instance().current_character_ptr();

		// Whether it runs in from afar when summoned
		bool bFromFar = lua_isboolean(lua, 1) ? lua_toboolean(lua, 1) : false;

		// vnum of the summoned creature
		uint32_t horseVnum= lua_isnumber(lua, 2) ? lua_tonumber(lua, 2) : 0;

		const char* name = lua_isstring(lua, 3) ? lua_tostring(lua, 3) : nullptr;
		ch->horse_summon(true, bFromFar, horseVnum, name);
		return 0;
	}

	int horse_unsummon(lua_State* lua)
	{
		character * ch = quest_manager::instance().current_character_ptr();
		ch->horse_summon(false);
		return 0;
	}

	int horse_is_mine(lua_State* lua)
	{
		character * ch = quest_manager::instance().current_character_ptr();
		character * horse = quest_manager::instance().current_npc_character_ptr();

		lua_pushboolean(lua, horse && horse->rider() == ch);
		return 1;
	}

	int horse_set_level(lua_State* lua)
	{
		character * ch = quest_manager::instance().current_character_ptr();

		if (!lua_isnumber(lua, 1))
			return 0;

		int newlevel = std::clamp((int)lua_tonumber(lua, 1), 0, HORSE_MAX_LEVEL);
		ch->set_horse_level(newlevel);
		ch->compute_points();
		ch->skill_level_packet();
		return 0;
	}

	int horse_get_level(lua_State* lua)
	{
		character * ch = quest_manager::instance().current_character_ptr();
		lua_pushnumber(lua, ch->horse_level());
		return 1;
	}

	int horse_advance(lua_State* lua)
	{
		character * ch = quest_manager::instance().current_character_ptr();

		if (ch->horse_level() >= HORSE_MAX_LEVEL)
			return 0;

		ch->set_horse_level(ch->horse_level() + 1);
		ch->compute_points();
		ch->skill_level_packet();
		return 0;
	}

	int horse_get_health(lua_State* lua)
	{
		character * ch = quest_manager::instance().current_character_ptr();

		if (ch->horse_level())
			lua_pushnumber(lua, ch->horse_health());
		else
			lua_pushnumber(lua, 0);

		return 1;
	}

	int horse_get_health_pct(lua_State* lua)
	{
		character * ch = quest_manager::instance().current_character_ptr();

		int pct = std::clamp(ch->horse_health() * 100 / ch->horse_max_health(), 0, 100);
		metin2::log::debug( "horse.get_health_pct {:d}", pct);

		if (ch->horse_level())
			lua_pushnumber(lua, pct);
		else
			lua_pushnumber(lua, 0);

		return 1;
	}

	int horse_get_stamina(lua_State* lua)
	{
		character * ch = quest_manager::instance().current_character_ptr();

		if (ch->horse_level())
			lua_pushnumber(lua, ch->horse_stamina());
		else
			lua_pushnumber(lua, 0);

		return 1;
	}

	int horse_get_stamina_pct(lua_State* lua)
	{
		character * ch = quest_manager::instance().current_character_ptr();
		int pct = std::clamp(ch->horse_stamina() * 100 / ch->horse_max_stamina(), 0, 100);
		metin2::log::debug( "horse.get_stamina_pct {:d}", pct);

		if (ch->horse_level())
			lua_pushnumber(lua, pct);
		else
			lua_pushnumber(lua, 0);

		return 1;
	}

	int horse_get_grade(lua_State* lua)
	{
		character * ch = quest_manager::instance().current_character_ptr();
		lua_pushnumber(lua, ch->horse_grade());
		return 1;
	}

	int horse_is_dead(lua_State* lua)
	{
		character * ch = quest_manager::instance().current_character_ptr();
		lua_pushboolean(lua, ch->horse_health()<=0);
		return 1;
	}

	int horse_revive(lua_State* lua)
	{
		character * ch = quest_manager::instance().current_character_ptr();
		if (ch->horse_level() > 0 && ch->horse_health() <= 0)
		{
			ch->revive_horse();
		}
		return 0;
	}

	int horse_feed(lua_State* lua)
	{
		character * ch = quest_manager::instance().current_character_ptr();
		if (ch->horse_level() > 0 && ch->horse_health() > 0)
		{
			ch->feed_horse();
		}
		return 0;
	}

	int horse_set_name(lua_State* lua)
	{
		// Return value
		// 0 : owns no horse
		// 1 : invalid name
		// 2 : rename succeeded

		if ( lua_isstring(lua, -1) != true ) return 0;

		character * ch = quest_manager::instance().current_character_ptr();

		if ( ch->horse_level() > 0 )
		{
			const char* pHorseName = lua_tostring(lua, -1);

			if ( pHorseName == nullptr || check_name(pHorseName) == 0 )
			{
				lua_pushnumber(lua, 1);
			}
			else
			{
				int nHorseNameDuration = metin2::config::test_server() == true ? 60*5 : 60*60*24*30;

				ch->set_quest_flag("horse_name.valid_till", get_global_time() + nHorseNameDuration);
				ch->add_affect(AFFECT_HORSE_NAME, 0, 0, 0, passes_per_sec(nHorseNameDuration), 0, true);

				horse_name_manager::instance().update_horse_name(ch->player_id(), lua_tostring(lua, -1), true);

				ch->horse_summon(false, true);
				ch->horse_summon(true, true);

				lua_pushnumber(lua, 2);
			}
		}
		else
		{
			lua_pushnumber(lua, 0);
		}

		return 1;
	}

	int horse_get_name(lua_State* lua)
	{
		character * ch = quest_manager::instance().current_character_ptr();

		if ( ch != nullptr )
		{
			const char* pHorseName = horse_name_manager::instance().horse_name(ch->player_id());

			if ( pHorseName != nullptr )
			{
				lua_pushstring(lua, pHorseName);
				return 1;
			}
		}

		lua_pushstring(lua, "");

		return 1;
	}

	void register_horse_function_table()
	{
		luaL_reg horse_functions[] =
		{
			{ "is_mine",		horse_is_mine			},
			{ "is_riding",		horse_is_riding			},
			{ "is_summon",		horse_is_summon			},
			{ "ride",			horse_ride				},
			{ "unride",			horse_unride			},
			{ "summon",			horse_summon			},
			{ "unsummon",		horse_unsummon			},
			{ "advance",		horse_advance			},
			{ "get_level",		horse_get_level			},
			{ "set_level",		horse_set_level			},
			{ "get_health",		horse_get_health		},
			{ "get_health_pct",	horse_get_health_pct	},
			{ "get_stamina",	horse_get_stamina		},
			{ "get_stamina_pct",horse_get_stamina_pct	},
			{ "get_grade",      horse_get_grade         },
			{ "is_dead",		horse_is_dead			},
			{ "revive",			horse_revive			},
			{ "feed",			horse_feed				},
			{ "set_name",		horse_set_name			},
			{ "get_name",		horse_get_name			},

			{ nullptr,				nullptr					}
		};

		quest_manager::instance().add_lua_function_table("horse", horse_functions);
	}
}
}
