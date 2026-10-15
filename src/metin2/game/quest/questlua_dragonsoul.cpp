
#include <metin2/game/characters/character.h>
#include <metin2/game/quest/lua_incl.h>
#include <metin2/game/quest/questmanager.h>

namespace metin2::game {


namespace quest 
{
	int ds_open_refine_window(lua_State* L)
	{
		character * const ch = quest_manager::instance().current_character_ptr();
		if (nullptr == ch)
		{
			quest::error("NULL POINT ERROR");
			return 0;
		}
		if (ch->dragon_soul_is_qualified())
			ch->dragon_soul_refine_window_open(quest_manager::instance().current_npc_character_ptr());

		return 0;
	}

	int ds_give_qualification(lua_State* L)
	{
		character * const ch = quest_manager::instance().current_character_ptr();
		if (nullptr == ch)
		{
			quest::error("NULL POINT ERROR");
			return 0;
		}
		ch->dragon_soul_give_qualification();

		return 0;
	}

	int ds_is_qualified(lua_State* L)
	{
		character * const ch = quest_manager::instance().current_character_ptr();
		if (nullptr == ch)
		{
			quest::error("NULL POINT ERROR");
			lua_pushnumber(L, 0);
			return 1;
		}

		lua_pushnumber(L, ch->dragon_soul_is_qualified());
		return 1;
	}

	void register_dragon_soul_function_table()
	{
		luaL_reg ds_functions[] = 
		{
			{ "open_refine_window"	, ds_open_refine_window },
			{ "give_qualification"	, ds_give_qualification },
			{ "is_qualified"		, ds_is_qualified		},
			{ nullptr					, nullptr					}
		};

		quest_manager::instance().add_lua_function_table("ds", ds_functions);
	}
};
}
