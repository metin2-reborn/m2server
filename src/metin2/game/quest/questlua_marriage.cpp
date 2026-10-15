#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/quest/lua_incl.h>
#include <metin2/game/social/marriage.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/social/wedding.h>

#include <metin2/proto/length.h>

#include <cstdint>

namespace metin2::game {



namespace quest
{
	int marriage_engage_to(lua_State* L)
	{
		uint32_t vid = (uint32_t) lua_tonumber(L, 1);
		character * ch = quest_manager::instance().current_character_ptr();
		character * ch_you = character_manager::instance().find(vid);
		if (ch_you)
		{
			marriage::manager::instance().request_add(ch->player_id(), ch_you->player_id(), ch->name(), ch_you->name());
		}
		return 0;
	}

	int marriage_remove(lua_State* L)
	{
		character * ch = quest_manager::instance().current_character_ptr();
		marriage::marriage* pMarriage = marriage::manager::instance().get(ch->player_id());
		if (!pMarriage)
		{
			quest::error("pid[{:d}:{}] is not exist couple", ch->player_id(), ch->name());
			return 0;
		}
		marriage::manager::instance().request_remove(ch->player_id(), pMarriage->other(ch->player_id()));
		return 0;
	}

	int marriage_set_to_marriage(lua_State* L)
	{
		character * ch = quest_manager::instance().current_character_ptr();
		marriage::marriage* pMarriage = marriage::manager::instance().get(ch->player_id());
		if (!pMarriage)
		{
			quest::error("pid[{:d}:{}] is not exist couple", ch->player_id(), ch->name());
			return 0;
		}
		pMarriage->set_married();
		return 0;
	}

	int marriage_find_married_vid(lua_State* L)
	{
		character * ch = quest_manager::instance().current_character_ptr();
		marriage::marriage* pMarriage = marriage::manager::instance().get(ch->player_id());
		uint32_t vid = 0;
		if (pMarriage)
		{
			character * you = character_manager::instance().find_by_pid(pMarriage->other(ch->player_id()));
			if (you)
				vid = you->get_vid();
		}

		lua_pushnumber(L, vid);

		return 1;
	}

	struct f_build_lua_wedding_map_list
	{
		lua_State * l;
		int count;
		f_build_lua_wedding_map_list(lua_State * L) : l(L), count(1)
		{
			lua_newtable(L);
		}

		void operator() (marriage::marriage* pMarriage)
		{
			if (!pMarriage->wedding_info)
				return;

			lua_newtable(l);
			lua_pushnumber(l, pMarriage->pid1);
			lua_rawseti(l, -2, 1);
			lua_pushnumber(l, pMarriage->pid2);
			lua_rawseti(l, -2, 2);
			lua_pushstring(l, pMarriage->name1.c_str());
			lua_rawseti(l, -2, 3);
			lua_pushstring(l, pMarriage->name2.c_str());
			lua_rawseti(l, -2, 4);
			lua_rawseti(l, -2, count++);
		}
	};

	int marriage_get_wedding_list(lua_State* L)
	{
		marriage::manager::instance().for_each_wedding(f_build_lua_wedding_map_list(L));
		return 1;
	}

	int marriage_join_wedding(lua_State* L)
	{
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2))
		{
			quest::error("invalid player id for wedding map");
			return 0;
		}

		uint32_t pid1 = (uint32_t) lua_tonumber(L, 1);
		uint32_t pid2 = (uint32_t) lua_tonumber(L, 2);

		character * ch = quest_manager::instance().current_character_ptr();

		marriage::marriage* pMarriage = marriage::manager::instance().get(pid1);
		if (!pMarriage)
		{
			quest::error("pid[{:d}:{}] is not exist couple", ch->player_id(), ch->name());
			return 0;
		}
		if (pMarriage->other(pid1) != pid2)
		{
			quest::error("not married {:d} {:d}", pid1, pid2);
			return 0;
		}
		//PREVENT_HACK
		if ( ch->is_hack() )
			return 0;
		//END_PREVENT_HACK

		pMarriage->warp_to_wedding_map(ch->player_id());
		return 0;
	}

	int marriage_warp_to_my_marriage_map(lua_State* L)
	{
		character * ch = quest_manager::instance().current_character_ptr();
		marriage::marriage* pMarriage = marriage::manager::instance().get(ch->player_id());
		if (!pMarriage)
		{
			quest::error("pid[{:d}:{}] is not exist couple", ch->player_id(), ch->name());
			return 0;
		}

		
		//PREVENT_HACK
		if ( ch->is_hack() )
			return 0;
		//END_PREVENT_HACK
		
		pMarriage->warp_to_wedding_map(ch->player_id());
		return 0;
	}

	int marriage_end_wedding(lua_State* L)
	{
		character * ch = quest_manager::instance().current_character_ptr();
		marriage::marriage* pMarriage = marriage::manager::instance().get(ch->player_id());
		if (!pMarriage)
		{
			quest::error("pid[{:d}:{}] is not exist couple", ch->player_id(), ch->name());
			return 0;
		}
		if (pMarriage->wedding_info)
		{
			// Request to end the wedding
			pMarriage->request_end_wedding();
		}
		return 0;
	}

	int marriage_wedding_dark(lua_State* L)
	{
		if (!lua_isboolean(L, 1))
		{
			quest::error("invalid argument 1 : must be boolean");
			return 0;
		}
		character * ch = quest_manager::instance().current_character_ptr();
		marriage::marriage* pMarriage = marriage::manager::instance().get(ch->player_id());

		if (!pMarriage)
		{
			quest::error("pid[{:d}:{}] is not exist couple", ch->player_id(), ch->name());
			return 0;
		}
		if (pMarriage->wedding_info)
		{
			marriage::wedding_map* pWedding = marriage::wedding_manager::instance().find(pMarriage->wedding_info->map_index);
			pWedding->set_dark(lua_toboolean(L, 1));
		}

		return 0;
	}

	int marriage_wedding_client_command(lua_State* L)
	{
		if (!lua_isstring(L, 1))
		{
			quest::error("invalid argument 1 : must be string");
			return 0;
		}

		character * ch = quest_manager::instance().current_character_ptr();
		marriage::marriage* pMarriage = marriage::manager::instance().get(ch->player_id());
		if (!pMarriage)
		{
			quest::error("pid[{:d}:{}] is not exist couple", ch->player_id(), ch->name());
			return 0;
		}
		if (pMarriage->wedding_info)
		{
			marriage::wedding_map* pWedding = marriage::wedding_manager::instance().find(pMarriage->wedding_info->map_index);
			pWedding->shout_in_map(CHAT_TYPE_COMMAND, lua_tostring(L, 1));
		}
		return 0;

	}

	int marriage_wedding_is_playing_music(lua_State* L)
	{
		character * ch = quest_manager::instance().current_character_ptr();
		marriage::marriage* pMarriage = marriage::manager::instance().get(ch->player_id());
		if (!pMarriage)
		{
			quest::error("pid[{:d}:{}] is not exist couple", ch->player_id(), ch->name());
			return 0;
		}
		if (pMarriage->wedding_info)
		{
			marriage::wedding_map* pWedding = marriage::wedding_manager::instance().find(pMarriage->wedding_info->map_index);
			if (pWedding)
				lua_pushboolean(L, pWedding->is_playing_music());
			else
				lua_pushboolean(L, false);
		}

		lua_pushboolean(L, false);
		return 1;
	}
	int marriage_wedding_music(lua_State* L)
	{
		if (!lua_isboolean(L, 1))
		{
			quest::error("invalid argument 1 : must be boolean");
			return 0;
		}
		if (!lua_isstring(L, 2))
		{
			quest::error("invalid argument 2 : must be string");
			return 0;
		}

		character * ch = quest_manager::instance().current_character_ptr();
		marriage::marriage* pMarriage = marriage::manager::instance().get(ch->player_id());
		if (!pMarriage)
		{
			quest::error("pid[{:d}:{}] is not exist couple", ch->player_id(), ch->name());
			return 0;
		}
		if (pMarriage->wedding_info)
		{
			marriage::wedding_map* pWedding = marriage::wedding_manager::instance().find(pMarriage->wedding_info->map_index);
			pWedding->set_music(
					lua_toboolean(L, 1),
					lua_tostring(L, 2)
					);
		}
		return 0;
	}
	int marriage_wedding_snow(lua_State* L)
	{
		if (!lua_isboolean(L, 1))
		{
			quest::error("invalid argument 1 : must be boolean");
			return 0;
		}
		character * ch = quest_manager::instance().current_character_ptr();
		marriage::marriage* pMarriage = marriage::manager::instance().get(ch->player_id());
		if (!pMarriage)
		{
			quest::error("pid[{:d}:{}] is not exist couple", ch->player_id(), ch->name());
			return 0;
		}
		if (pMarriage->wedding_info)
		{
			marriage::wedding_map* pWedding = marriage::wedding_manager::instance().find(pMarriage->wedding_info->map_index);
			pWedding->set_snow(lua_toboolean(L, 1));
		}
		return 0;
	}

	int marriage_in_my_wedding(lua_State* L)
	{
		character * ch = quest_manager::instance().current_character_ptr();
		marriage::marriage* pMarriage = marriage::manager::instance().get(ch->player_id());
		if (pMarriage->wedding_info)
		{
			lua_pushboolean(L, (uint32_t)ch->map_index() == pMarriage->wedding_info->map_index);
		}
		else
		{
			lua_pushboolean(L, 0);
		}
		return 1;
	}

	int marriage_get_married_time(lua_State* L)
	{
		character * ch = quest_manager::instance().current_character_ptr();
		marriage::marriage* pMarriage = marriage::manager::instance().get(ch->player_id());

		if (!pMarriage)
		{
			quest::error("trying to get time for not married character");
			lua_pushnumber(L, 0);
			return 1;
		}

		lua_pushnumber(L, get_global_time() - pMarriage->marry_time);
		return 1;
	}

	void register_marriage_function_table()
	{
		luaL_reg marriage_functions[] = 
		{
			{ "engage_to",		marriage_engage_to	    },
			{ "remove",			marriage_remove		    },
			{ "find_married_vid",	marriage_find_married_vid   },
			{ "get_wedding_list",	marriage_get_wedding_list   },
			{ "join_wedding",		marriage_join_wedding	    },
			{ "set_to_marriage",	marriage_set_to_marriage    },
			{ "end_wedding",		marriage_end_wedding	    },
			{ "wedding_dark",		marriage_wedding_dark	    },
			{ "wedding_snow",		marriage_wedding_snow	    },
			{ "wedding_music",		marriage_wedding_music	    },
			{ "wedding_is_playing_music", marriage_wedding_is_playing_music },
			{ "wedding_client_command",	marriage_wedding_client_command    },
			{ "in_my_wedding",		marriage_in_my_wedding	    },
			{ "warp_to_my_marriage_map",marriage_warp_to_my_marriage_map},
			{ "get_married_time",	marriage_get_married_time   },
			{ nullptr,			nullptr			    }
		};
		quest_manager::instance().add_lua_function_table("marriage", marriage_functions);
	}
}
}
