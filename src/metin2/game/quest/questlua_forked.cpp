
#include <metin2/game/characters/character.h>
#include <metin2/game/app/config.h>
#include <metin2/game/world/entity.h>
#include <metin2/game/app/event.h>
#include <metin2/game/quest/lua_incl.h>
#include <metin2/game/quest/questlua.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/world/sectree_manager.h>
#include <metin2/game/empire/threeway_war.h>
#include <metin2/game/app/typedef.h>

#include <metin2/log/log.h>
#include <metin2/core/utils.h>

namespace metin2::game {


namespace quest
{

	int forked_set_dead_count( lua_State* lua )
	{
		character * ch = quest_manager::instance().current_character_ptr();

		quest_manager& q = quest_manager::instance();

		if (nullptr != ch)
		{
			three_way_war::instance().set_revive_token_for_player( ch->player_id(), q.event_flag("threeway_war_dead_count") );
		}

		return 0; 
	}

	int forked_get_dead_count( lua_State* lua )
	{
		character * ch = quest_manager::instance().current_character_ptr();

		if (nullptr != ch)
		{
			lua_pushnumber( lua, three_way_war::instance().revive_token_for_player(ch->player_id()) );
		}
		else
		{
			lua_pushnumber( lua, 0 );
		}

		return 1; 
	}

	int forked_init_kill_count_per_empire( lua_State* lua )
	{
		character * ch = quest_manager::instance().current_character_ptr();

		three_way_war::instance().set_kill_score( ch->empire(), 0 );

		return 0;
	}

	int forked_init( lua_State* lua )
	{
		three_way_war::instance().initialize();
		three_way_war::instance().random_event_map_set();

		return 0;
	}

	int forked_sungzi_start_pos( lua_State* lua )
	{
		character * ch = quest_manager::instance().current_character_ptr();

		const forked_sungzi_map_info& info = three_way_war::instance().event_sung_zi_map_info();

		lua_pushnumber( lua, info.forked_sungzi_start_position[ch->empire()-1][0] );
		lua_pushnumber( lua, info.forked_sungzi_start_position[ch->empire()-1][1] );

		return 2;
	}

	int forked_pass_start_pos( lua_State* lua )
	{
		character * ch = quest_manager::instance().current_character_ptr();

		const forked_pass_map_info& info = three_way_war::instance().event_pass_map_info();

		lua_pushnumber( lua, info.forked_pass_start_position[ch->empire()-1][0] );
		lua_pushnumber( lua, info.forked_pass_start_position[ch->empire()-1][1] );

		return 2;
	}

	int forked_sungzi_mapindex (lua_State *lua )
	{
		lua_pushnumber( lua, get_sungzi_map_index() ); 

		if ( metin2::config::test_server() )
			metin2::log::info( "forked_sungzi_map_index_by_empire {:d}", get_sungzi_map_index() );
		return 1;
	}

	int forked_pass_mapindex_by_empire( lua_State *lua )
	{
		lua_pushnumber( lua, get_pass_map_index(lua_tonumber(lua,1)) ); 

		return 1;
	}

	int forked_get_pass_path_by_empire( lua_State *lua )
	{
		int iEmpire 	= (int)lua_tonumber(lua, 1);

		lua_pushstring( lua, get_pass_map_path(iEmpire) );
		metin2::log::info( "[PASS_PATH] Empire {:d} Path  {}", iEmpire, get_pass_map_path( iEmpire ) );
		return 1;
	}

	int forked_is_forked_mapindex( lua_State * lua )
	{
		lua_pushboolean( lua, three_way_war::instance().is_three_way_war_map_index(lua_tonumber(lua,1)) );

		return 1;
	}

	int forked_is_sungzi_mapindex( lua_State * lua )
	{
		lua_pushboolean( lua, three_way_war::instance().is_sung_zi_map_index(lua_tonumber(lua,1)) );

		return 1;
	}

	struct f_warp_in_map
	{
		int map_index_to;
		int x;
		int y;

		void operator()(entity * ent)
		{
			if (ent->is_type(ENTITY_CHARACTER))
			{
				character * ch = (character *) ent;
				if (ch->is_pc())
				{
					ch->warp_set( x+(number(0,5)*100), y+(number(0,5)*100), map_index_to);
				}
			}
		}
	};

	EVENTINFO(warp_all_to_map_event_info)
	{
		int		map_index_from;
		int 	map_index_to;
		int 	x;
		int		y;

		warp_all_to_map_event_info()
		: map_index_from( 0 )
		, map_index_to( 0 )
		, x( 0 )
		, y( 0 )
		{
		}
	};

	EVENTFUNC(warp_all_to_map_event)
	{
		warp_all_to_map_event_info * info = dynamic_cast<warp_all_to_map_event_info *>(event->info);

		if ( info == nullptr )
		{
			metin2::log::error( "warp_all_to_map_event> <Factor> Null pointer" );
			return 0;
		}

		sectree_map * pSecMap = sectree_manager::instance().map( info->map_index_from );
		if ( pSecMap )
		{
			f_warp_in_map f;
			f.map_index_to = info->map_index_to;
			f.x			= info->x;
			f.y			= info->y;
			pSecMap->for_each( f );
		}

		return 0;
	}

	int forked_warp_all_in_map( lua_State * lua )
	{
		int iMapIndexFrom	= (int)lua_tonumber(lua, 1 );
		int iMapIndexTo		= (int)lua_tonumber(lua, 2 );
		int ix				= (int)lua_tonumber(lua, 3 );
		int iy				= (int)lua_tonumber(lua, 4 );
		int iTime			= (int)lua_tonumber(lua, 5 );

		warp_all_to_map_event_info* info = alloc_event_info<warp_all_to_map_event_info>();

		info->map_index_from	= iMapIndexFrom;
		info->map_index_to		= iMapIndexTo;
		info->x				= ix;
		info->y				= iy;

		event_create(warp_all_to_map_event, info, passes_per_sec(iTime));

		return 0;
	}	

	int forked_is_registered_user( lua_State* lua )
	{
		character * ch = quest_manager::instance().current_character_ptr();

		if (nullptr != ch)
		{
			lua_pushboolean( lua, three_way_war::instance().is_registered_user(ch->player_id()) );
		}
		else
		{
			lua_pushboolean( lua, false );
		}

		return 1;
	}

	int forked_register_user( lua_State* lua )
	{
		character * ch = quest_manager::instance().current_character_ptr();

		if (nullptr != ch)
		{
			three_way_war::instance().register_user( ch->player_id() );
		}

		return 0;
	}

	int forked_purge_all_monsters( lua_State* lua )
	{
		three_way_war::instance().remove_all_monsters_in_three_way();

		return 0;
	}

	void register_forked_function_table() 
	{
		luaL_reg forked_functions[] = 
		{
			{ "setdeadcount",			forked_set_dead_count				},
			{ "getdeadcount",			forked_get_dead_count				},
			{ "initkillcount",			forked_init_kill_count_per_empire	},
			{ "initforked",				forked_init							},
			{ "get_sungzi_start_pos",	forked_sungzi_start_pos				},
			{ "get_pass_start_pos",		forked_pass_start_pos				},
			{ "getsungzimapindex",		forked_sungzi_mapindex				},
			{ "getpassmapindexbyempire", forked_pass_mapindex_by_empire		},
			{ "getpasspathbyempire",	forked_get_pass_path_by_empire		},
			{ "isforkedmapindex",		forked_is_forked_mapindex			},
			{ "issungzimapindex",		forked_is_sungzi_mapindex			},
			{ "warp_all_in_map",		forked_warp_all_in_map				},
			{ "is_registered_user",		forked_is_registered_user			},
			{ "register_user",			forked_register_user				},
			{ "purge_all_monsters",		forked_purge_all_monsters			},

			{ nullptr,				nullptr			}
		};

		quest_manager::instance().add_lua_function_table("forked", forked_functions);
	}
}
}
