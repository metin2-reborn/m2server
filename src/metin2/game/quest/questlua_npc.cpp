#include <metin2/game/characters/char.h>
#include <metin2/game/characters/char_manager.h>
#include <metin2/game/guilds/guild.h>
#include <metin2/game/quest/lua_incl.h>
#include <metin2/game/parties/party.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/trade/shop_manager.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/events/xmas_event.h>

#include <metin2/log/log.h>

#include <algorithm>
#include <cstdint>
#include <string>

namespace metin2::game {

namespace quest
{
	//
	// "npc" lua functions
	//
	int npc_open_shop(lua_State * L)
	{
		int iShopVnum = 0;

		if (lua_gettop(L) == 1)
		{
			if (lua_isnumber(L, 1))
				iShopVnum = (int) lua_tonumber(L, 1);
		}

		if (quest_manager::instance().current_npc_character_ptr())
			shop_manager::instance().start_shopping(quest_manager::instance().current_character_ptr(), quest_manager::instance().current_npc_character_ptr(), iShopVnum);
		return 0;
	}

	int npc_is_pc(lua_State* L)
	{
		quest_manager& q = quest_manager::instance();
		character * npc = q.current_npc_character_ptr();
		if (npc && npc->is_pc())
			lua_pushboolean(L, 1);
		else
			lua_pushboolean(L, 0);
		return 1;
	}

	int npc_get_empire(lua_State * L)
	{
		quest_manager& q = quest_manager::instance();
		character * npc = q.current_npc_character_ptr();
		if (npc)
			lua_pushnumber(L, npc->empire());
		else
			lua_pushnumber(L, 0);
		return 1;
	}

	int npc_get_race(lua_State * L)
	{
		lua_pushnumber(L, quest_manager::instance().current_npc_race());
		return 1;
	}

	int npc_get_guild(lua_State* L)
	{
		quest_manager& q = quest_manager::instance();
		character * npc = q.current_npc_character_ptr();
		guild* pGuild = nullptr;
		if (npc)
			pGuild = npc->get_guild();

		lua_pushnumber(L, pGuild ? pGuild->id() : 0);
		return 1;
	}

	int npc_get_remain_skill_book_count(lua_State* L)
	{
		character * npc = quest_manager::instance().current_npc_character_ptr();
		if (!npc || npc->is_pc() || npc->race_num() != xmas::MOB_SANTA_VNUM)
		{
			lua_pushnumber(L, 0);
			return 1;
		}
		lua_pushnumber(L, std::max(0, npc->point(POINT_ATT_GRADE_BONUS)));
		return 1;
	}

	int npc_dec_remain_skill_book_count(lua_State* L)
	{
		character * npc = quest_manager::instance().current_npc_character_ptr();
		if (!npc || npc->is_pc() || npc->race_num() != xmas::MOB_SANTA_VNUM)
		{
			return 0;
		}
		npc->set_point(POINT_ATT_GRADE_BONUS, std::max(0, npc->point(POINT_ATT_GRADE_BONUS)-1));
		return 0;
	}

	int npc_get_remain_hairdye_count(lua_State* L)
	{
		character * npc = quest_manager::instance().current_npc_character_ptr();
		if (!npc || npc->is_pc() || npc->race_num() != xmas::MOB_SANTA_VNUM)
		{
			lua_pushnumber(L, 0);
			return 1;
		}
		lua_pushnumber(L, std::max(0, npc->point(POINT_DEF_GRADE_BONUS)));
		return 1;
	}

	int npc_dec_remain_hairdye_count(lua_State* L)
	{
		character * npc = quest_manager::instance().current_npc_character_ptr();
		if (!npc || npc->is_pc() || npc->race_num() != xmas::MOB_SANTA_VNUM)
		{
			return 0;
		}
		npc->set_point(POINT_DEF_GRADE_BONUS, std::max(0, npc->point(POINT_DEF_GRADE_BONUS)-1));
		return 0;
	}

	int npc_is_quest(lua_State * L)
	{
		quest_manager& q = quest_manager::instance();
		character * npc = q.current_npc_character_ptr();

		if (npc)
		{
			const std::string & r_st = q.current_quest_name();

			if (q.quest_index_by_name(r_st) == npc->quest_by())
			{
				lua_pushboolean(L, 1);
				return 1;
			}
		}

		lua_pushboolean(L, 0);
		return 1;
	}

	int npc_kill(lua_State * L)
	{
		quest_manager& q = quest_manager::instance();
		character * ch = q.current_character_ptr();
		character * npc = q.current_npc_character_ptr();

		ch->set_quest_npcid(0);
		if (npc)
		{
			npc->dead();
		}
		return 0;
	}

	int npc_purge(lua_State * L)
	{
		quest_manager& q = quest_manager::instance();
		character * ch = q.current_character_ptr();
		character * npc = q.current_npc_character_ptr();

		ch->set_quest_npcid(0);
		if (npc)
		{
			character_manager::instance().destroy_character(npc);
		}
		return 0;
	}

	int npc_is_near(lua_State * L)
	{
		quest_manager& q = quest_manager::instance();
		character * ch = q.current_character_ptr();
		character * npc = q.current_npc_character_ptr();

		lua_Number dist = 10;

		if (lua_isnumber(L, 1))
			dist = lua_tonumber(L, 1);

		if (ch == nullptr || npc == nullptr)
		{
			lua_pushboolean(L, false);
		}
		else
		{
			lua_pushboolean(L, distance_approx(ch->x() - npc->x(), ch->y() - npc->y()) < dist*100);
		}

		return 1;
	}

	int npc_is_near_vid(lua_State* L)
	{
		if (!lua_isnumber(L, 1))
		{
			metin2::log::error("invalid vid");
			lua_pushboolean(L, 0);
			return 1;
		}

		quest_manager& q = quest_manager::instance();
		character * ch = character_manager::instance().find((uint32_t)lua_tonumber(L, 1));
		character * npc = q.current_npc_character_ptr();

		lua_Number dist = 10;

		if (lua_isnumber(L, 2))
			dist = lua_tonumber(L, 2);

		if (ch == nullptr || npc == nullptr)
		{
			lua_pushboolean(L, false);
		}
		else
		{
			lua_pushboolean(L, distance_approx(ch->x() - npc->x(), ch->y() - npc->y()) < dist*100);
		}

		return 1;
	}

	int npc_unlock(lua_State* L)
	{
		quest_manager& q = quest_manager::instance();
		character * ch = q.current_character_ptr();
		character * npc = q.current_npc_character_ptr();

		if ( npc != nullptr )
		{
			if (npc->is_pc())
				return 0;

			if (npc->quest_npcid() == ch->player_id())
			{
				npc->set_quest_npcid(0);
			}
		}
		return 0;
	}

	int npc_lock(lua_State* L)
	{
		quest_manager& q = quest_manager::instance();
		character * ch = q.current_character_ptr();
		character * npc = q.current_npc_character_ptr();

		if (!npc || npc->is_pc())
		{
			lua_pushboolean(L, true);
			return 1;
		}

		if (npc->quest_npcid() == 0 || npc->quest_npcid() == ch->player_id())
		{
			npc->set_quest_npcid(ch->player_id());
			lua_pushboolean(L, true);
		}
		else
		{
			lua_pushboolean(L, false);
		}

		return 1;
	}

	int npc_get_leader_vid(lua_State* L)
	{
		quest_manager& q = quest_manager::instance();
		character * npc = q.current_npc_character_ptr();

		party * party = npc->get_party();
		character * leader = party->leader();

		if (leader)
			lua_pushnumber(L, leader->get_vid());
		else
			lua_pushnumber(L, 0);


		return 1;
	}

	int npc_get_vid(lua_State* L)
	{
		quest_manager& q = quest_manager::instance();
		character * npc = q.current_npc_character_ptr();
		
		lua_pushnumber(L, npc->get_vid());


		return 1;
	}

	int npc_get_vid_attack_mul(lua_State* L)
	{

		lua_Number vid = lua_tonumber(L, 1);
		character * targetChar = character_manager::instance().find(vid);

		if (targetChar)
			lua_pushnumber(L, targetChar->att_mul());
		else
			lua_pushnumber(L, 0);


		return 1;
	}
	
	int npc_set_vid_attack_mul(lua_State* L)
	{

		lua_Number vid = lua_tonumber(L, 1);
		lua_Number attack_mul = lua_tonumber(L, 2);

		character * targetChar = character_manager::instance().find(vid);

		if (targetChar)
			targetChar->set_att_mul(attack_mul);

		return 0;
	}

	int npc_get_vid_damage_mul(lua_State* L)
	{

		lua_Number vid = lua_tonumber(L, 1);
		character * targetChar = character_manager::instance().find(vid);

		if (targetChar)
			lua_pushnumber(L, targetChar->dam_mul());
		else
			lua_pushnumber(L, 0);


		return 1;
	}
	
	int npc_set_vid_damage_mul(lua_State* L)
	{

		lua_Number vid = lua_tonumber(L, 1);
		lua_Number damage_mul = lua_tonumber(L, 2);

		character * targetChar = character_manager::instance().find(vid);

		if (targetChar)
			targetChar->set_dam_mul(damage_mul);

		return 0;
	}

	void register_npc_function_table()
	{
		luaL_reg npc_functions[] = 
		{
			{ "getrace",			npc_get_race			},
			{ "get_race",			npc_get_race			},
			{ "open_shop",			npc_open_shop			},
			{ "get_empire",			npc_get_empire			},
			{ "is_pc",				npc_is_pc			},
			{ "is_quest",			npc_is_quest			},
			{ "kill",				npc_kill			},
			{ "purge",				npc_purge			},
			{ "is_near",			npc_is_near			},
			{ "is_near_vid",			npc_is_near_vid			},
			{ "lock",				npc_lock			},
			{ "unlock",				npc_unlock			},
			{ "get_guild",			npc_get_guild			},
			{ "get_leader_vid",		npc_get_leader_vid	},
			{ "get_vid",			npc_get_vid	},
			{ "get_vid_attack_mul",		npc_get_vid_attack_mul	},
			{ "set_vid_attack_mul",		npc_set_vid_attack_mul	},
			{ "get_vid_damage_mul",		npc_get_vid_damage_mul	},
			{ "set_vid_damage_mul",		npc_set_vid_damage_mul	},

			// X-mas santa special
			{ "get_remain_skill_book_count",	npc_get_remain_skill_book_count },
			{ "dec_remain_skill_book_count",	npc_dec_remain_skill_book_count },
			{ "get_remain_hairdye_count",	npc_get_remain_hairdye_count	},
			{ "dec_remain_hairdye_count",	npc_dec_remain_hairdye_count	},
			{ nullptr,				nullptr			    	}
		};

		quest_manager::instance().add_lua_function_table("npc", npc_functions);
	}
};
}
