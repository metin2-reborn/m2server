#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/guilds/guild.h>
#include <metin2/game/guilds/guild_manager.h>
#include <metin2/game/quest/lua_incl.h>
#include <metin2/game/quest/questlua.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>
#include <metin2/log/log.h>

#include <cstdint>
#include <cstring>
#include <vector>

namespace metin2::game {


namespace quest
{
	int guild_around_ranking_string(lua_State* lua)
	{
		quest_manager& q = quest_manager::instance();
		character * ch = q.current_character_ptr();
		if (!ch->get_guild())
			lua_pushstring(lua,"");
		else
		{
			char szBuf[4096+1];
			guild_manager::instance().around_rank_string(ch->get_guild()->id(), szBuf, sizeof(szBuf));
			lua_pushstring(lua, szBuf);
		}
		return 1;
	}

	int guild_high_ranking_string(lua_State* lua)
	{
		quest_manager& q = quest_manager::instance();
		character * ch = q.current_character_ptr();
		uint32_t dwMyGuild = 0;
		if (ch->get_guild())
			dwMyGuild = ch->get_guild()->id();

		char szBuf[4096+1];
		guild_manager::instance().high_rank_string(dwMyGuild, szBuf, sizeof(szBuf));
		lua_pushstring(lua, szBuf);
		return 1;
	}

	int guild_get_ladder_point(lua_State* lua)
	{
		quest_manager& q = quest_manager::instance();
		character * ch = q.current_character_ptr();
		if (!ch->get_guild())
		{
			lua_pushnumber(lua, -1);
		}
		else
		{
			lua_pushnumber(lua, ch->get_guild()->ladder_point());
		}
		return 1;
	}

	int guild_get_rank(lua_State* lua)
	{
		quest_manager& q = quest_manager::instance();
		character * ch = q.current_character_ptr();

		if (!ch->get_guild())
		{
			lua_pushnumber(lua, -1);
		}
		else
		{
			lua_pushnumber(lua, guild_manager::instance().rank(ch->get_guild()));
		}
		return 1;
	}

	int guild_is_war(lua_State* lua)
	{
		if (!lua_isnumber(lua, 1))
		{
			quest::error("invalid argument");
			return 0;
		}

		character * ch = quest_manager::instance().current_character_ptr();

		if (ch->get_guild() && ch->get_guild()->under_war((uint32_t) lua_tonumber(lua, 1)))
			lua_pushboolean(lua, true);
		else
			lua_pushboolean(lua, false);

		return 1;
	}

	int guild_name(lua_State* lua)
	{
		if (!lua_isnumber(lua, 1))
		{
			quest::error("invalid argument");
			return 0;
		}

		guild * pkGuild = guild_manager::instance().find_guild((uint32_t) lua_tonumber(lua, 1));

		if (pkGuild)
			lua_pushstring(lua, pkGuild->name());
		else
			lua_pushstring(lua, "");

		return 1;
	}

	int guild_level(lua_State* lua)
	{
		luaL_checknumber(lua, 1);

		guild * pkGuild = guild_manager::instance().find_guild((uint32_t) lua_tonumber(lua, 1));

		if (pkGuild)
			lua_pushnumber(lua, pkGuild->level());
		else
			lua_pushnumber(lua, 0);

		return 1;
	}

	int guild_war_enter(lua_State* lua)
	{
		if (!lua_isnumber(lua, 1))
		{
			quest::error("invalid argument");
			return 0;
		}

		quest_manager& q = quest_manager::instance();
		character * ch = q.current_character_ptr();

		if (ch->get_guild())
			ch->get_guild()->guild_war_entry_accept((uint32_t) lua_tonumber(lua, 1), ch);

		return 0;
	}

	int guild_get_any_war(lua_State* lua)
	{
		character * ch = quest_manager::instance().current_character_ptr();

		if (ch->get_guild())
			lua_pushnumber(lua, ch->get_guild()->under_any_war());
		else
			lua_pushnumber(lua, 0);

		return 1;
	}

	int guild_get_name(lua_State * lua)
	{
		if (!lua_isnumber(lua, 1))
		{
			lua_pushstring(lua,  "");
			return 1;
		}

		guild * pkGuild = guild_manager::instance().find_guild((uint32_t) lua_tonumber(lua, 1));

		if (pkGuild)
			lua_pushstring(lua, pkGuild->name());
		else
			lua_pushstring(lua, "");

		return 1;
	}

	int guild_war_bet(lua_State * lua)
	{
		if (!lua_isnumber(lua, 1) || !lua_isnumber(lua, 2) || !lua_isnumber(lua, 3))
		{
			quest::error("invalid argument");
			return 0;
		}

		character * ch = quest_manager::instance().current_character_ptr();

		packet_gd_guild_war_bet p;

		p.war_id = (uint32_t) lua_tonumber(lua, 1);
		strlcpy(p.login, ch->get_desc()->get_account_table().login, sizeof(p.login));
		p.guild = (uint32_t) lua_tonumber(lua, 2);
		p.gold = (uint32_t) lua_tonumber(lua, 3);

		metin2::log::info( "GUILD_WAR_BET: {} login {} war_id {:d} guild {:d} gold {:d}", 
				ch->name(), p.login, p.war_id, p.guild, p.gold);

		db_clientdesc->db_packet(HEADER_GD_GUILD_WAR_BET, 0, &p, sizeof(p));
		return 0;
	}

	int guild_is_bet(lua_State * lua)
	{
		if (!lua_isnumber(lua, 1))
		{
			quest::error("invalid argument");
			lua_pushboolean(lua, true);
			return 1;
		}

		bool bBet = guild_manager::instance().is_bet((uint32_t) lua_tonumber(lua, 1),
				quest_manager::instance().current_character_ptr()->get_desc()->get_account_table().login);

		lua_pushboolean(lua, bBet);
		return 1;
	}

	int guild_get_warp_war_list(lua_State* lua)
	{
		f_build_lua_guild_war_list f(lua);
		guild_manager::instance().for_each_war(f);
		return 1;
	}

	int guild_get_reserve_war_table(lua_State * lua)
	{
		std::vector<guild_war_reserve_for_game *> & con = guild_manager::instance().reserve_war_ref();

		int i = 0;
		std::vector<guild_war_reserve_for_game *>::iterator it = con.begin();

		metin2::log::info( "con.size(): {:d}", con.size());

		lua_newtable(lua);

		while (it != con.end())
		{
			guild_war_reserve * p = &(*(it++))->data;

			if (p->type != GUILD_WAR_TYPE_BATTLE)
				continue;

			lua_newtable(lua);

			metin2::log::info( "con.size(): {:d} {:d} {:d} handi {:d}", p->id, p->guild_from, p->guild_to, p->handicap);

			lua_pushnumber(lua, p->id);
			lua_rawseti(lua, -2, 1);

			if (p->power_from > p->power_to)
				lua_pushnumber(lua, p->guild_from);
			else
				lua_pushnumber(lua, p->guild_to);
			lua_rawseti(lua, -2, 2);

			if (p->power_from > p->power_to)
				lua_pushnumber(lua, p->guild_to);
			else
				lua_pushnumber(lua, p->guild_from);
			lua_rawseti(lua, -2, 3);

			lua_pushnumber(lua, p->handicap);
			lua_rawseti(lua, -2, 4);

			lua_rawseti(lua, -2, ++i);
		}

		return 1;
	}

	int guild_get_member_count(lua_State* lua)
	{
		character * ch = quest_manager::instance().current_character_ptr();
		
		if ( ch == nullptr )
		{
			lua_pushnumber(lua, 0);
			return 1;
		}

		guild* pGuild = ch->get_guild();

		if ( pGuild == nullptr )
		{
			lua_pushnumber(lua, 0);
			return 1;
		}

		lua_pushnumber(lua, pGuild->member_count());

		return 1;
	}

	int guild_change_master(lua_State* lua)
	{

		character * ch = quest_manager::instance().current_character_ptr();

		guild* pGuild = ch->get_guild();

		if ( pGuild != nullptr )
		{
			if ( pGuild->master_pid() == ch->player_id() )
			{
				if ( lua_isstring(lua, 1) == false )
				{
					lua_pushnumber(lua, 0);
				}
				else
				{
					bool ret = pGuild->change_master_to(pGuild->member_pid(lua_tostring(lua, 1)));

					lua_pushnumber(lua, ret == false ? 2 : 3 );
				}
			}
			else
			{
				lua_pushnumber(lua, 1);
			}
		}
		else
		{
			lua_pushnumber(lua, 4);
		}

		return 1;
	}

	int guild_change_master_with_limit(lua_State* lua)
	{

		character * ch = quest_manager::instance().current_character_ptr();

		guild* pGuild = ch->get_guild();

		if ( pGuild != nullptr )
		{
			if ( pGuild->master_pid() == ch->player_id() )
			{
				if ( lua_isstring(lua, 1) == false )
				{
					lua_pushnumber(lua, 0);
				}
				else
				{
					character * pNewMaster = character_manager::instance().find_pc( lua_tostring(lua,1) );

					if ( pNewMaster != nullptr )
					{
						if ( pNewMaster->level() < lua_tonumber(lua, 2) )
						{
							lua_pushnumber(lua, 6);
						}
						else
						{
							int nBeOtherLeader = pNewMaster->quest_flag("change_guild_master.be_other_leader");
							quest_manager::instance().get_pc( ch->player_id() );

							if ( lua_toboolean(lua, 6) == true ) nBeOtherLeader = 0;

							if ( nBeOtherLeader > get_global_time() )
							{
								lua_pushnumber(lua, 7);
							}
							else
							{
								bool ret = pGuild->change_master_to(pGuild->member_pid(lua_tostring(lua, 1)));

								if ( ret == false )
								{
									lua_pushnumber(lua, 2);
								}
								else
								{
									lua_pushnumber(lua, 3);

									pNewMaster->set_quest_flag("change_guild_master.be_other_leader", 0);
									pNewMaster->set_quest_flag("change_guild_master.be_other_member", 0);
									pNewMaster->set_quest_flag("change_guild_master.resign_limit", (int)lua_tonumber(lua, 3));

									ch->set_quest_flag("change_guild_master.be_other_leader", (int)lua_tonumber(lua, 4));
									ch->set_quest_flag("change_guild_master.be_other_member", (int)lua_tonumber(lua, 5));
									ch->set_quest_flag("change_guild_master.resign_limit", 0);
								}
							}
						}
					}
					else
					{
						lua_pushnumber(lua, 5);
					}
				}
			}
			else
			{
				lua_pushnumber(lua, 1);
			}
		}
		else
		{
			lua_pushnumber(lua, 4);
		}

		return 1;
	}

	void register_guild_function_table()
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

		quest_manager::instance().add_lua_function_table("guild", guild_functions);
	}
}
}
