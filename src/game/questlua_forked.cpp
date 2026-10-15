
#include "char.h"
#include "config.h"
#include "entity.h"
#include "event.h"
#include "lua_incl.h"
#include "questlua.h"
#include "questmanager.h"
#include "sectree_manager.h"
#include "threeway_war.h"
#include "typedef.h"

#include <metin2/log/log.h>
#include <libthecore/utils.h>

namespace metin2::game {


namespace quest
{
	//
	// "forked_" lua functions
	//

	int forked_set_dead_count( lua_State* L )
	{
		CHARACTER * ch = CQuestManager::instance().GetCurrentCharacterPtr();

		CQuestManager& q = CQuestManager::instance();

		if (nullptr != ch)
		{
			CThreeWayWar::instance().SetReviveTokenForPlayer( ch->GetPlayerID(), q.GetEventFlag("threeway_war_dead_count") );
		}

		return 0; 
	}

	int forked_get_dead_count( lua_State* L )
	{
		CHARACTER * ch = CQuestManager::instance().GetCurrentCharacterPtr();

		if (nullptr != ch)
		{
			lua_pushnumber( L, CThreeWayWar::instance().GetReviveTokenForPlayer(ch->GetPlayerID()) );
		}
		else
		{
			lua_pushnumber( L, 0 );
		}

		return 1; 
	}

	int forked_init_kill_count_per_empire( lua_State* L )
	{
		CHARACTER * ch = CQuestManager::instance().GetCurrentCharacterPtr();

		CThreeWayWar::instance().SetKillScore( ch->GetEmpire(), 0 );

		return 0;
	}

	int forked_init( lua_State* L )
	{
		CThreeWayWar::instance().Initialize();
		CThreeWayWar::instance().RandomEventMapSet();

		return 0;
	}

	int forked_sungzi_start_pos( lua_State* L )
	{
		CHARACTER * ch = CQuestManager::instance().GetCurrentCharacterPtr();

		const ForkedSungziMapInfo& info = CThreeWayWar::instance().GetEventSungZiMapInfo();

		lua_pushnumber( L, info.m_iForkedSungziStartPosition[ch->GetEmpire()-1][0] );
		lua_pushnumber( L, info.m_iForkedSungziStartPosition[ch->GetEmpire()-1][1] );

		return 2;
	}

	int forked_pass_start_pos( lua_State* L )
	{
		CHARACTER * ch = CQuestManager::instance().GetCurrentCharacterPtr();

		const ForkedPassMapInfo& info = CThreeWayWar::instance().GetEventPassMapInfo();

		lua_pushnumber( L, info.m_iForkedPassStartPosition[ch->GetEmpire()-1][0] );
		lua_pushnumber( L, info.m_iForkedPassStartPosition[ch->GetEmpire()-1][1] );

		return 2;
	}

	int forked_sungzi_mapindex (lua_State *L )
	{
		lua_pushnumber( L, GetSungziMapIndex() ); 

		if ( metin2::config::test_server() )
			metin2::log::info( "forked_sungzi_map_index_by_empire {:d}", GetSungziMapIndex() );
		return 1;
	}

	int forked_pass_mapindex_by_empire( lua_State *L )
	{
		lua_pushnumber( L, GetPassMapIndex(lua_tonumber(L,1)) ); 

		return 1;
	}

	int forked_get_pass_path_by_empire( lua_State *L )
	{
		int iEmpire 	= (int)lua_tonumber(L, 1);

		lua_pushstring( L, GetPassMapPath(iEmpire) );
		metin2::log::info( "[PASS_PATH] Empire {:d} Path  {}", iEmpire, GetPassMapPath( iEmpire ) );
		return 1;
	}

	int forked_is_forked_mapindex( lua_State * L )
	{
		lua_pushboolean( L, CThreeWayWar::instance().IsThreeWayWarMapIndex(lua_tonumber(L,1)) );

		return 1;
	}

	int forked_is_sungzi_mapindex( lua_State * L )
	{
		lua_pushboolean( L, CThreeWayWar::instance().IsSungZiMapIndex(lua_tonumber(L,1)) );

		return 1;
	}

	struct FWarpInMap
	{
		int m_iMapIndexTo;
		int m_x;
		int m_y;

		void operator()(CEntity * ent)
		{
			if (ent->IsType(ENTITY_CHARACTER))
			{
				CHARACTER * ch = (CHARACTER *) ent;
				if (ch->IsPC())
				{
					ch->WarpSet( m_x+(number(0,5)*100), m_y+(number(0,5)*100), m_iMapIndexTo);
				}
			}
		}
	};

	EVENTINFO(warp_all_to_map_event_info)
	{
		int		m_iMapIndexFrom;
		int 	m_iMapIndexTo;
		int 	m_x;
		int		m_y;

		warp_all_to_map_event_info()
		: m_iMapIndexFrom( 0 )
		, m_iMapIndexTo( 0 )
		, m_x( 0 )
		, m_y( 0 )
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

		SECTREE_MAP * pSecMap = SECTREE_MANAGER::instance().GetMap( info->m_iMapIndexFrom );
		if ( pSecMap )
		{
			FWarpInMap f;
			f.m_iMapIndexTo = info->m_iMapIndexTo;
			f.m_x			= info->m_x;
			f.m_y			= info->m_y;
			pSecMap->for_each( f );
		}

		return 0;
	}

	int forked_warp_all_in_map( lua_State * L )
	{
		int iMapIndexFrom	= (int)lua_tonumber(L, 1 );
		int iMapIndexTo		= (int)lua_tonumber(L, 2 );
		int ix				= (int)lua_tonumber(L, 3 );
		int iy				= (int)lua_tonumber(L, 4 );
		int iTime			= (int)lua_tonumber(L, 5 );

		warp_all_to_map_event_info* info = AllocEventInfo<warp_all_to_map_event_info>();

		info->m_iMapIndexFrom	= iMapIndexFrom;
		info->m_iMapIndexTo		= iMapIndexTo;
		info->m_x				= ix;
		info->m_y				= iy;

		event_create(warp_all_to_map_event, info, PASSES_PER_SEC(iTime));

		return 0;
	}	

	int forked_is_registered_user( lua_State* L )
	{
		CHARACTER * ch = CQuestManager::instance().GetCurrentCharacterPtr();

		if (nullptr != ch)
		{
			lua_pushboolean( L, CThreeWayWar::instance().IsRegisteredUser(ch->GetPlayerID()) );
		}
		else
		{
			lua_pushboolean( L, false );
		}

		return 1;
	}

	int forked_register_user( lua_State* L )
	{
		CHARACTER * ch = CQuestManager::instance().GetCurrentCharacterPtr();

		if (nullptr != ch)
		{
			CThreeWayWar::instance().RegisterUser( ch->GetPlayerID() );
		}

		return 0;
	}

	int forked_purge_all_monsters( lua_State* L )
	{
		CThreeWayWar::instance().RemoveAllMonstersInThreeWay();

		return 0;
	}

	void RegisterForkedFunctionTable() 
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

		CQuestManager::instance().AddLuaFunctionTable("forked", forked_functions);
	}
}
}
