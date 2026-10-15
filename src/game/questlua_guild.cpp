#include "char.h"
#include "char_manager.h"
#include "desc_client.h"
#include "guild.h"
#include "guild_manager.h"
#include "lua_incl.h"
#include "questlua.h"
#include "questmanager.h"
#include "typedef.h"
#include "utils.h"

#include <common/length.h>
#include <common/tables.h>
#include <metin2/log/log.h>

#include <cstdint>
#include <cstring>
#include <vector>


namespace quest
{
	//
	// "guild" Lua functions
	//
	int guild_around_ranking_string(lua_State* L)
	{
		CQuestManager& q = CQuestManager::instance();
		CHARACTER * ch = q.GetCurrentCharacterPtr();
		if (!ch->GetGuild())
			lua_pushstring(L,"");
		else
		{
			char szBuf[4096+1];
			CGuildManager::instance().GetAroundRankString(ch->GetGuild()->GetID(), szBuf, sizeof(szBuf));
			lua_pushstring(L, szBuf);
		}
		return 1;
	}

	int guild_high_ranking_string(lua_State* L)
	{
		CQuestManager& q = CQuestManager::instance();
		CHARACTER * ch = q.GetCurrentCharacterPtr();
		uint32_t dwMyGuild = 0;
		if (ch->GetGuild())
			dwMyGuild = ch->GetGuild()->GetID();

		char szBuf[4096+1];
		CGuildManager::instance().GetHighRankString(dwMyGuild, szBuf, sizeof(szBuf));
		lua_pushstring(L, szBuf);
		return 1;
	}

	int guild_get_ladder_point(lua_State* L)
	{
		CQuestManager& q = CQuestManager::instance();
		CHARACTER * ch = q.GetCurrentCharacterPtr();
		if (!ch->GetGuild())
		{
			lua_pushnumber(L, -1);
		}
		else
		{
			lua_pushnumber(L, ch->GetGuild()->GetLadderPoint());
		}
		return 1;
	}

	int guild_get_rank(lua_State* L)
	{
		CQuestManager& q = CQuestManager::instance();
		CHARACTER * ch = q.GetCurrentCharacterPtr();

		if (!ch->GetGuild())
		{
			lua_pushnumber(L, -1);
		}
		else
		{
			lua_pushnumber(L, CGuildManager::instance().GetRank(ch->GetGuild()));
		}
		return 1;
	}

	int guild_is_war(lua_State* L)
	{
		if (!lua_isnumber(L, 1))
		{
			quest::error("invalid argument");
			return 0;
		}

		CHARACTER * ch = CQuestManager::instance().GetCurrentCharacterPtr();

		if (ch->GetGuild() && ch->GetGuild()->UnderWar((uint32_t) lua_tonumber(L, 1)))
			lua_pushboolean(L, true);
		else
			lua_pushboolean(L, false);

		return 1;
	}

	int guild_name(lua_State* L)
	{
		if (!lua_isnumber(L, 1))
		{
			quest::error("invalid argument");
			return 0;
		}

		CGuild * pkGuild = CGuildManager::instance().FindGuild((uint32_t) lua_tonumber(L, 1));

		if (pkGuild)
			lua_pushstring(L, pkGuild->GetName());
		else
			lua_pushstring(L, "");

		return 1;
	}

	int guild_level(lua_State* L)
	{
		luaL_checknumber(L, 1);

		CGuild * pkGuild = CGuildManager::instance().FindGuild((uint32_t) lua_tonumber(L, 1));

		if (pkGuild)
			lua_pushnumber(L, pkGuild->GetLevel());
		else
			lua_pushnumber(L, 0);

		return 1;
	}

	int guild_war_enter(lua_State* L)
	{
		if (!lua_isnumber(L, 1))
		{
			quest::error("invalid argument");
			return 0;
		}

		CQuestManager& q = CQuestManager::instance();
		CHARACTER * ch = q.GetCurrentCharacterPtr();

		if (ch->GetGuild())
			ch->GetGuild()->GuildWarEntryAccept((uint32_t) lua_tonumber(L, 1), ch);

		return 0;
	}

	int guild_get_any_war(lua_State* L)
	{
		CHARACTER * ch = CQuestManager::instance().GetCurrentCharacterPtr();

		if (ch->GetGuild())
			lua_pushnumber(L, ch->GetGuild()->UnderAnyWar());
		else
			lua_pushnumber(L, 0);

		return 1;
	}

	int guild_get_name(lua_State * L)
	{
		if (!lua_isnumber(L, 1))
		{
			lua_pushstring(L,  "");
			return 1;
		}

		CGuild * pkGuild = CGuildManager::instance().FindGuild((uint32_t) lua_tonumber(L, 1));

		if (pkGuild)
			lua_pushstring(L, pkGuild->GetName());
		else
			lua_pushstring(L, "");

		return 1;
	}

	int guild_war_bet(lua_State * L)
	{
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3))
		{
			quest::error("invalid argument");
			return 0;
		}

		CHARACTER * ch = CQuestManager::instance().GetCurrentCharacterPtr();

		TPacketGDGuildWarBet p;

		p.dwWarID = (uint32_t) lua_tonumber(L, 1);
		strlcpy(p.szLogin, ch->GetDesc()->GetAccountTable().login, sizeof(p.szLogin));
		p.dwGuild = (uint32_t) lua_tonumber(L, 2);
		p.dwGold = (uint32_t) lua_tonumber(L, 3);

		metin2::log::info( "GUILD_WAR_BET: {} login {} war_id {:d} guild {:d} gold {:d}", 
				ch->GetName(), p.szLogin, p.dwWarID, p.dwGuild, p.dwGold);

		db_clientdesc->DBPacket(HEADER_GD_GUILD_WAR_BET, 0, &p, sizeof(p));
		return 0;
	}

	int guild_is_bet(lua_State * L)
	{
		if (!lua_isnumber(L, 1))
		{
			quest::error("invalid argument");
			lua_pushboolean(L, true);
			return 1;
		}

		bool bBet = CGuildManager::instance().IsBet((uint32_t) lua_tonumber(L, 1),
				CQuestManager::instance().GetCurrentCharacterPtr()->GetDesc()->GetAccountTable().login);

		lua_pushboolean(L, bBet);
		return 1;
	}

	int guild_get_warp_war_list(lua_State* L)
	{
		FBuildLuaGuildWarList f(L);
		CGuildManager::instance().for_each_war(f);
		return 1;
	}

	int guild_get_reserve_war_table(lua_State * L)
	{
		std::vector<CGuildWarReserveForGame *> & con = CGuildManager::instance().GetReserveWarRef();

		int i = 0;
		std::vector<CGuildWarReserveForGame *>::iterator it = con.begin();

		metin2::log::info( "con.size(): {:d}", con.size());

		// stack : table1
		lua_newtable(L);

		while (it != con.end())
		{
			TGuildWarReserve * p = &(*(it++))->data;

			if (p->bType != GUILD_WAR_TYPE_BATTLE)
				continue;

			lua_newtable(L);

			metin2::log::info( "con.size(): {:d} {:d} {:d} handi {:d}", p->dwID, p->dwGuildFrom, p->dwGuildTo, p->lHandicap);

			// stack : table1 table2
			lua_pushnumber(L, p->dwID);
			// stack : table1 table2 dwID
			lua_rawseti(L, -2, 1);

			// stack : table1 table2
			if (p->lPowerFrom > p->lPowerTo)
				lua_pushnumber(L, p->dwGuildFrom);
			else
				lua_pushnumber(L, p->dwGuildTo);
			// stack : table1 table2 guildfrom
			lua_rawseti(L, -2, 2);

			// stack : table1 table2
			if (p->lPowerFrom > p->lPowerTo)
				lua_pushnumber(L, p->dwGuildTo);
			else
				lua_pushnumber(L, p->dwGuildFrom);
			// stack : table1 table2 guildto
			lua_rawseti(L, -2, 3);

			lua_pushnumber(L, p->lHandicap);
			lua_rawseti(L, -2, 4);

			// stack : table1 table2
			lua_rawseti(L, -2, ++i);
		}

		return 1;
	}

	int guild_get_member_count(lua_State* L)
	{
		CHARACTER * ch = CQuestManager::instance().GetCurrentCharacterPtr();
		
		if ( ch == nullptr )
		{
			lua_pushnumber(L, 0);
			return 1;
		}

		CGuild* pGuild = ch->GetGuild();

		if ( pGuild == nullptr )
		{
			lua_pushnumber(L, 0);
			return 1;
		}

		lua_pushnumber(L, pGuild->GetMemberCount());

		return 1;
	}

	int guild_change_master(lua_State* L)
	{
		// Return values
		//	0 : the given name is invalid ( not a string )
		//	1 : not the guild leader
		//	2 : no guild member with the given name
		//	3 : request succeeded
		//	4 : no guild

		CHARACTER * ch = CQuestManager::instance().GetCurrentCharacterPtr();

		CGuild* pGuild = ch->GetGuild();

		if ( pGuild != nullptr )
		{
			if ( pGuild->GetMasterPID() == ch->GetPlayerID() )
			{
				if ( lua_isstring(L, 1) == false )
				{
					lua_pushnumber(L, 0);
				}
				else
				{
					bool ret = pGuild->ChangeMasterTo(pGuild->GetMemberPID(lua_tostring(L, 1)));

					lua_pushnumber(L, ret == false ? 2 : 3 );
				}
			}
			else
			{
				lua_pushnumber(L, 1);
			}
		}
		else
		{
			lua_pushnumber(L, 4);
		}

		return 1;
	}

	int guild_change_master_with_limit(lua_State* L)
	{
		// Arguments
		//  arg0 : new guild leader name
		//  arg1 : new guild leader level requirement
		//  arg2 : resign_limit time limit
		//  arg3 : be_other_leader time limit
		//  arg4 : be_other_member time limit
		//  arg5 : whether it is a cash item
		//
		// Return values
		//	0 : the given name is invalid ( not a string )
		//	1 : not the guild leader
		//	2 : no guild member with the given name
		//	3 : request succeeded
		//	4 : no guild
		//	5 : the named character is not online
		//	6 : the named character's level is below the required level
		//	7 : the new guild leader is blocked by the be_other_leader limit

		CHARACTER * ch = CQuestManager::instance().GetCurrentCharacterPtr();

		CGuild* pGuild = ch->GetGuild();

		if ( pGuild != nullptr )
		{
			if ( pGuild->GetMasterPID() == ch->GetPlayerID() )
			{
				if ( lua_isstring(L, 1) == false )
				{
					lua_pushnumber(L, 0);
				}
				else
				{
					CHARACTER * pNewMaster = CHARACTER_MANAGER::instance().FindPC( lua_tostring(L,1) );

					if ( pNewMaster != nullptr )
					{
						if ( pNewMaster->GetLevel() < lua_tonumber(L, 2) )
						{
							lua_pushnumber(L, 6);
						}
						else
						{
							int nBeOtherLeader = pNewMaster->GetQuestFlag("change_guild_master.be_other_leader");
							CQuestManager::instance().GetPC( ch->GetPlayerID() );

							if ( lua_toboolean(L, 6) == true ) nBeOtherLeader = 0;

							if ( nBeOtherLeader > get_global_time() )
							{
								lua_pushnumber(L, 7);
							}
							else
							{
								bool ret = pGuild->ChangeMasterTo(pGuild->GetMemberPID(lua_tostring(L, 1)));

								if ( ret == false )
								{
									lua_pushnumber(L, 2);
								}
								else
								{
									lua_pushnumber(L, 3);

									pNewMaster->SetQuestFlag("change_guild_master.be_other_leader", 0);
									pNewMaster->SetQuestFlag("change_guild_master.be_other_member", 0);
									pNewMaster->SetQuestFlag("change_guild_master.resign_limit", (int)lua_tonumber(L, 3));

									ch->SetQuestFlag("change_guild_master.be_other_leader", (int)lua_tonumber(L, 4));
									ch->SetQuestFlag("change_guild_master.be_other_member", (int)lua_tonumber(L, 5));
									ch->SetQuestFlag("change_guild_master.resign_limit", 0);
								}
							}
						}
					}
					else
					{
						lua_pushnumber(L, 5);
					}
				}
			}
			else
			{
				lua_pushnumber(L, 1);
			}
		}
		else
		{
			lua_pushnumber(L, 4);
		}

		return 1;
	}

	void RegisterGuildFunctionTable()
	{
		luaL_reg guild_functions[] =
		{
			{ "get_rank",				guild_get_rank				},
			{ "get_ladder_point",		guild_get_ladder_point		},
			{ "high_ranking_string",	guild_high_ranking_string	},
			{ "around_ranking_string",	guild_around_ranking_string	},
			{ "name",					guild_name					},
			{ "level",					guild_level					},
			{ "is_war",					guild_is_war				},
			{ "war_enter",				guild_war_enter				},
			{ "get_any_war",			guild_get_any_war			},
			{ "get_reserve_war_table",	guild_get_reserve_war_table	},
			{ "get_name",				guild_get_name				},
			{ "war_bet",				guild_war_bet				},
			{ "is_bet",					guild_is_bet				},
			{ "get_warp_war_list",		guild_get_warp_war_list		},
			{ "get_member_count",		guild_get_member_count		},
			{ "change_master",			guild_change_master			},
			{ "change_master_with_limit",			guild_change_master_with_limit			},

			{ nullptr,						nullptr						}
		};

		CQuestManager::instance().AddLuaFunctionTable("guild", guild_functions);
	}
}

