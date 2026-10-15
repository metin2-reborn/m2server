#include <metin2/game/protocol/buffer_manager.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/world/dungeon.h>
#include <metin2/game/quest/lua_incl.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/parties/party.h>
#include <metin2/game/quest/questlua.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/world/sectree_manager.h>
#include <metin2/game/app/typedef.h>

#include <metin2/log/log.h>
#include <metin2/core/utils.h>

#include <cmath>
#include <cstdint>
#include <numbers>
#include <string>
#include <utility>

namespace metin2::game {



template <class Func> Func dungeon::for_each_member(Func f)
{
	decltype(set_pk_character_.begin()) it;

	for (it = set_pk_character_.begin(); it != set_pk_character_.end(); ++it)
	{
		metin2::log::info( "Dungeon ForEachMember {}", (*it)->name());
		f(*it);
	}
	return f;
}

namespace quest
{
	//
	// "dungeon" lua functions
	//
	int dungeon_notice(lua_State* lua)
	{
		if (!lua_isstring(lua, 1))
			return 0;

		quest_manager& q = quest_manager::instance();
		dungeon * pDungeon = q.current_dungeon();

		if (pDungeon)
			pDungeon->notice(lua_tostring(lua, 1));
		return 0;
	}

	int dungeon_set_quest_flag(lua_State* lua)
	{
		quest_manager & q = quest_manager::instance();

		f_set_quest_flag f;

		f.flagname = q.current_pc()->current_quest_name() + "." + lua_tostring(lua, 1);
		f.value = (int) rint(lua_tonumber(lua, 2));

		dungeon * pDungeon = q.current_dungeon();

		if (pDungeon)
			pDungeon->for_each_member(f);

		return 0;
	}

	int dungeon_set_flag(lua_State* lua)
	{
		if (!lua_isstring(lua,1) || !lua_isnumber(lua,2))
		{
			quest::error("wrong set flag");
		}
		else
		{
			quest_manager& q = quest_manager::instance();
			dungeon * pDungeon = q.current_dungeon();

			if (pDungeon)
			{
				const char* sz = lua_tostring(lua,1);
				int value = int(lua_tonumber(lua, 2));
				pDungeon->set_flag(sz, value);
			}
			else
			{
				quest::error("no dungeon !!!");
			}
		}
		return 0;
	}

	int dungeon_get_flag(lua_State* lua)
	{
		if (!lua_isstring(lua,1))
		{
			quest::error("wrong get flag");
		}

		quest_manager& q = quest_manager::instance();
		dungeon * pDungeon = q.current_dungeon();

		if (pDungeon)
		{
			const char* sz = lua_tostring(lua,1);
			lua_pushnumber(lua, pDungeon->flag(sz));
		}
		else
		{
			quest::error("no dungeon !!!");
			lua_pushnumber(lua, 0);
		}

		return 1;
	}

	int dungeon_get_flag_from_map_index(lua_State* lua)
	{
		if (!lua_isstring(lua,1) || !lua_isnumber(lua,2))
		{
			quest::error("wrong get flag");
		}

		uint32_t dwMapIndex = (uint32_t) lua_tonumber(lua, 2);
		if (dwMapIndex)
		{
			dungeon * pDungeon = dungeon_manager::instance().find_by_map_index(dwMapIndex);
			if (pDungeon)
			{
				const char* sz = lua_tostring(lua,1);
				lua_pushnumber(lua, pDungeon->flag(sz));
			}
			else
			{
				quest::error("no dungeon !!!");
				lua_pushnumber(lua, 0);
			}
		}
		else
		{
			lua_pushboolean(lua, 0);
		}
		return 1;
	}

	int dungeon_get_map_index(lua_State* lua)
	{
		quest_manager& q = quest_manager::instance();
		dungeon * pDungeon = q.current_dungeon();

		if (pDungeon)
		{
			metin2::log::info( "Dungeon GetMapIndex {:d}",pDungeon->map_index());
			lua_pushnumber(lua, pDungeon->map_index());
		}
		else
		{
			quest::error("no dungeon !!!");
			lua_pushnumber(lua, 0);
		}

		return 1;
	}

	int dungeon_regen_file(lua_State* lua)
	{
		if (!lua_isstring(lua,1))
		{
			quest::error("wrong filename");
			return 0;
		}

		quest_manager& q = quest_manager::instance();
		dungeon * pDungeon = q.current_dungeon();

		if (pDungeon)
			pDungeon->spawn_regen(lua_tostring(lua,1));

		return 0;
	}

	int dungeon_set_regen_file(lua_State* lua)
	{
		if (!lua_isstring(lua,1))
		{
			quest::error("wrong filename");
			return 0;
		}
		quest_manager& q = quest_manager::instance();
		dungeon * pDungeon = q.current_dungeon();

		if (pDungeon)
			pDungeon->spawn_regen(lua_tostring(lua,1), false);
		return 0;
	}

	int dungeon_clear_regen(lua_State* lua)
	{
		quest_manager& q = quest_manager::instance();
		dungeon * pDungeon = q.current_dungeon();
		if (pDungeon)
			pDungeon->clear_regen();
		return 0;
	}

	int dungeon_check_eliminated(lua_State* lua)
	{
		quest_manager& q = quest_manager::instance();
		dungeon * pDungeon = q.current_dungeon();
		if (pDungeon)
			pDungeon->check_eliminated();
		return 0;
	}

	int dungeon_set_exit_all_at_eliminate(lua_State* lua)
	{
		if (!lua_isnumber(lua,1))
		{
			quest::error("wrong time");
			return 0;
		}

		quest_manager& q = quest_manager::instance();
		dungeon * pDungeon = q.current_dungeon();

		if (pDungeon)
			pDungeon->set_exit_all_at_eliminate((int32_t)lua_tonumber(lua, 1));

		return 0;
	}

	int dungeon_set_warp_at_eliminate(lua_State* lua)
	{
		if (!lua_isnumber(lua, 1))
		{
			quest::error("wrong time");
			return 0;
		}

		if (!lua_isnumber(lua, 2))
		{
			quest::error("wrong map index");
			return 0;
		}

		if (!lua_isnumber(lua, 3)) 
		{
			quest::error("wrong X");
			return 0;
		}

		if (!lua_isnumber(lua, 4))
		{
			quest::error("wrong Y");
			return 0;
		}

		const char * c_pszRegenFile = nullptr;

		if (lua_gettop(lua) >= 5)
			c_pszRegenFile = lua_tostring(lua,5);

		quest_manager& q = quest_manager::instance();
		dungeon * pDungeon = q.current_dungeon();

		if (pDungeon)
		{
			pDungeon->set_warp_at_eliminate((int32_t)lua_tonumber(lua,1),
										 (int32_t)lua_tonumber(lua,2),
										 (int32_t)lua_tonumber(lua,3),
										 (int32_t)lua_tonumber(lua,4),
										 c_pszRegenFile);
		}
		else
			quest::error("cannot find dungeon");

		return 0;
	}

	int dungeon_new_jump(lua_State* lua)
	{
		if (lua_gettop(lua) < 3)
		{
			quest::error("not enough argument");
			return 0;
		}

		if (!lua_isnumber(lua, 1) || !lua_isnumber(lua, 2) || !lua_isnumber(lua, 3))
		{
			quest::error("wrong argument");
			return 0;
		}

		int32_t lMapIndex = (int32_t)lua_tonumber(lua,1);

		dungeon * pDungeon = dungeon_manager::instance().create(lMapIndex);

		if (!pDungeon)
		{
			quest::error("cannot create dungeon {:d}", lMapIndex);
			return 0;
		}

		character * ch = quest_manager::instance().current_character_ptr();

		ch->warp_set((int) lua_tonumber(lua, 2), (int)lua_tonumber(lua, 3), pDungeon->map_index());
		return 0;
	}

	int dungeon_new_jump_all(lua_State* lua)
	{
		if (lua_gettop(lua)<3 || !lua_isnumber(lua,1) || !lua_isnumber(lua, 2) || !lua_isnumber(lua,3))
		{
			quest::error("not enough argument");
			return 0;
		}

		int32_t lMapIndex = (int32_t)lua_tonumber(lua,1);

		dungeon * pDungeon = dungeon_manager::instance().create(lMapIndex);

		if (!pDungeon)
		{
			quest::error("cannot create dungeon {:d}", lMapIndex);
			return 0;
		}

		character * ch = quest_manager::instance().current_character_ptr();

		pDungeon->jump_all(ch->map_index(), (int)lua_tonumber(lua, 2), (int)lua_tonumber(lua, 3));

		return 0;
	}

	int dungeon_new_jump_party (lua_State* lua)
	{
		if (lua_gettop(lua)<3 || !lua_isnumber(lua,1) || !lua_isnumber(lua, 2) || !lua_isnumber(lua,3))
		{
			quest::error("not enough argument");
			return 0;
		}

		int32_t lMapIndex = (int32_t)lua_tonumber(lua,1);

		dungeon * pDungeon = dungeon_manager::instance().create(lMapIndex);

		if (!pDungeon)
		{
			quest::error("cannot create dungeon {:d}", lMapIndex);
			return 0;
		}

		character * ch = quest_manager::instance().current_character_ptr();

		if (ch->get_party() == nullptr)
		{
			quest::error("cannot go to dungeon alone.");
			return 0;
		}
		pDungeon->jump_party(ch->get_party(), ch->map_index(), (int)lua_tonumber(lua, 2), (int)lua_tonumber(lua, 3));

		return 0;
	}

	int dungeon_jump_all(lua_State* lua)
	{
		if (lua_gettop(lua)<2 || !lua_isnumber(lua, 1) || !lua_isnumber(lua,2))
			return 0;

		dungeon * pDungeon = quest_manager::instance().current_dungeon();

		if (!pDungeon)
			return 0;

		pDungeon->jump_all(pDungeon->map_index(), (int)lua_tonumber(lua, 1), (int)lua_tonumber(lua, 2));
		return 0;
	}

	int dungeon_warp_all(lua_State* lua)
	{
		if (lua_gettop(lua)<2 || !lua_isnumber(lua, 1) || !lua_isnumber(lua,2))
			return 0;

		dungeon * pDungeon = quest_manager::instance().current_dungeon();

		if (!pDungeon)
			return 0;

		pDungeon->warp_all(pDungeon->map_index(), (int)lua_tonumber(lua, 1), (int)lua_tonumber(lua, 2));
		return 0;
	}

	int dungeon_get_kill_stone_count(lua_State* lua)
	{
		if (!lua_isnumber(lua,1) || !lua_isnumber(lua,2))
		{
			lua_pushnumber(lua, 0);
			return 1;
		}


		quest_manager& q = quest_manager::instance();
		dungeon * pDungeon = q.current_dungeon();

		if (pDungeon)
		{
			lua_pushnumber(lua, pDungeon->kill_stone_count());
			return 1;
		}

		lua_pushnumber(lua, 0);
		return 1;
	}

	int dungeon_get_kill_mob_count(lua_State * lua)
	{
		if (!lua_isnumber(lua,1) || !lua_isnumber(lua,2))
		{
			lua_pushnumber(lua, 0);
			return 1;
		}

		quest_manager& q = quest_manager::instance();
		dungeon * pDungeon = q.current_dungeon();

		if (pDungeon)
		{
			lua_pushnumber(lua, pDungeon->kill_mob_count());
			return 1;
		}

		lua_pushnumber(lua, 0);
		return 1;
	}

	int dungeon_is_use_potion(lua_State* lua)
	{
		if (!lua_isnumber(lua, 1) || !lua_isnumber(lua, 2))
		{
			lua_pushboolean(lua, 1);
			return 1;
		}

		quest_manager& q = quest_manager::instance();
		dungeon * pDungeon = q.current_dungeon();

		if (pDungeon)
		{
			lua_pushboolean(lua, pDungeon->is_use_potion());
			return 1;
		}

		lua_pushboolean(lua, 1);
		return 1;
	}

	int dungeon_revived(lua_State* lua) 
	{
		if (!lua_isnumber(lua,1) || !lua_isnumber(lua,2))
		{
			lua_pushboolean(lua, 1);
			return 1;
		}

		quest_manager& q = quest_manager::instance();
		dungeon * pDungeon = q.current_dungeon();

		if (pDungeon)
		{
			lua_pushboolean(lua, pDungeon->is_use_revive());
			return 1;
		}

		lua_pushboolean(lua, 1);
		return 1;
	}

	int dungeon_set_dest(lua_State* lua)
	{
		if (!lua_isnumber(lua,1) || !lua_isnumber(lua,2))
			return 0;

		character * ch = quest_manager::instance().current_character_ptr();

		party * pParty = ch->get_party();
		quest_manager& q = quest_manager::instance();
		dungeon * pDungeon = q.current_dungeon();

		if (pDungeon && pParty)
			pDungeon->send_dest_position_to_party(pParty, (int)lua_tonumber(lua,1), (int)lua_tonumber(lua,2));

		return 0;
	}

	int dungeon_unique_set_maxhp(lua_State* lua)
	{
		if (!lua_isstring(lua,1) || !lua_isnumber(lua,2))
			return 0;

		quest_manager& q = quest_manager::instance();
		dungeon * pDungeon = q.current_dungeon();

		if (pDungeon)
			pDungeon->unique_set_max_hp(lua_tostring(lua,1), (int)lua_tonumber(lua,2));

		return 0;
	}

	int dungeon_unique_set_hp(lua_State* lua)
	{
		if (!lua_isstring(lua,1) || !lua_isnumber(lua,2))
			return 0;

		quest_manager& q = quest_manager::instance();
		dungeon * pDungeon = q.current_dungeon();

		if (pDungeon)
			pDungeon->unique_set_hp(lua_tostring(lua,1), (int)lua_tonumber(lua,2));

		return 0;
	}

	int dungeon_unique_set_def_grade(lua_State* lua)
	{
		if (!lua_isstring(lua,1) || !lua_isnumber(lua,2))
			return 0;

		quest_manager& q = quest_manager::instance();
		dungeon * pDungeon = q.current_dungeon();

		if (pDungeon)
			pDungeon->unique_set_def_grade(lua_tostring(lua,1), (int)lua_tonumber(lua,2));

		return 0;
	}

	int dungeon_unique_get_hp_perc(lua_State* lua)
	{
		if (!lua_isstring(lua,1))
		{
			lua_pushnumber(lua,0);
			return 1;
		}

		quest_manager& q = quest_manager::instance();
		dungeon * pDungeon = q.current_dungeon();

		if (pDungeon)
		{
			lua_pushnumber(lua, pDungeon->unique_hp_perc(lua_tostring(lua,1)));
			return 1;
		}

		lua_pushnumber(lua,0);
		return 1;
	}

	int dungeon_is_unique_dead(lua_State* lua)
	{
		if (!lua_isstring(lua,1))
		{
			lua_pushboolean(lua, 0);
			return 1;
		}

		quest_manager& q = quest_manager::instance();
		dungeon * pDungeon = q.current_dungeon();

		if (pDungeon)
		{
			lua_pushboolean(lua, pDungeon->is_unique_dead(lua_tostring(lua,1))?1:0);
			return 1;
		}

		lua_pushboolean(lua, 0);
		return 1;
	}

	int dungeon_purge_unique(lua_State* lua)
	{
		if (!lua_isstring(lua,1))
			return 0;
		metin2::log::info("QUEST_DUNGEON_PURGE_UNIQUE {}", lua_tostring(lua,1));

		quest_manager& q = quest_manager::instance();
		dungeon * pDungeon = q.current_dungeon();

		if (pDungeon)
			pDungeon->purge_unique(lua_tostring(lua,1));

		return 0;
	}

	struct f_purge_area
	{
		int x1, y1, x2, y2;
		character * except_char;

		f_purge_area(int a, int b, int c, int d, character * p)
			: x1(a), y1(b), x2(c), y2(d),
			except_char(p)
		{}

		void operator () (entity * ent)
		{
			if (true == ent->is_type(ENTITY_CHARACTER))
			{
				character * pChar = static_cast<character *>(ent);

				if (pChar == except_char)
					return;
					
				if (!pChar->is_pet() && (true == pChar->is_monster() || true == pChar->is_stone()))
				{
					if (x1 <= pChar->x() && pChar->x() <= x2 && y1 <= pChar->y() && pChar->y() <= y2)
					{
						character_manager::instance().destroy_character(pChar);
					}
				}
			}
		}
	};
	
	int dungeon_purge_area(lua_State* lua)
	{
		if (!lua_isnumber(lua,1) || !lua_isnumber(lua,2) || !lua_isnumber(lua,3) || !lua_isnumber(lua,4))
			return 0;
		metin2::log::info("QUEST_DUNGEON_PURGE_AREA");

		int x1 = lua_tonumber(lua, 1);
		int y1 = lua_tonumber(lua, 2);
		int x2 = lua_tonumber(lua, 3);
		int y2 = lua_tonumber(lua, 4);

		quest_manager& q = quest_manager::instance();
		dungeon * pDungeon = q.current_dungeon();

		const int mapIndex = pDungeon->map_index();

		if (0 == mapIndex)
		{
			quest::error("_purge_area: cannot get a map index with ({:d}, {:d})", x1, y1);
			return 0;
		}

		sectree_map * pSectree = sectree_manager::instance().map(mapIndex);

		if (nullptr != pSectree)
		{
			f_purge_area func(x1, y1, x2, y2, quest_manager::instance().current_npc_character_ptr());

			pSectree->for_each(func);
		}

		return 0;
	}

	int dungeon_kill_unique(lua_State* lua)
	{
		if (!lua_isstring(lua,1))
			return 0;
		metin2::log::info("QUEST_DUNGEON_KILL_UNIQUE {}", lua_tostring(lua,1));

		quest_manager& q = quest_manager::instance();
		dungeon * pDungeon = q.current_dungeon();

		if (pDungeon)
			pDungeon->kill_unique(lua_tostring(lua,1));

		return 0;
	}

	int dungeon_spawn_stone_door(lua_State* lua)
	{
		if (!lua_isstring(lua,1) || !lua_isstring(lua,2))
			return 0;
		metin2::log::info("QUEST_DUNGEON_SPAWN_STONE_DOOR {} {}", lua_tostring(lua,1), lua_tostring(lua,2));

		quest_manager& q = quest_manager::instance();
		dungeon * pDungeon = q.current_dungeon();

		if (pDungeon)
			pDungeon->spawn_stone_door(lua_tostring(lua,1), lua_tostring(lua,2));

		return 0;
	}

	int dungeon_spawn_wooden_door(lua_State* lua)
	{
		if (!lua_isstring(lua,1) || !lua_isstring(lua,2))
			return 0;
		metin2::log::info("QUEST_DUNGEON_SPAWN_WOODEN_DOOR {} {}", lua_tostring(lua,1), lua_tostring(lua,2));

		quest_manager& q = quest_manager::instance();
		dungeon * pDungeon = q.current_dungeon();

		if (pDungeon)
			pDungeon->spawn_wooden_door(lua_tostring(lua,1), lua_tostring(lua,2));

		return 0;
	}

	int dungeon_spawn_move_group(lua_State* lua)
	{
		if (!lua_isnumber(lua,1) || !lua_isstring(lua,2) || !lua_isstring(lua,3))
			return 0;
		metin2::log::info("QUEST_DUNGEON_SPAWN_MOVE_GROUP {:d} {} {}", (int)lua_tonumber(lua,1), lua_tostring(lua,2), lua_tostring(lua,3));

		quest_manager& q = quest_manager::instance();
		dungeon * pDungeon = q.current_dungeon();

		if (pDungeon)
			pDungeon->spawn_move_group((uint32_t)lua_tonumber(lua,1), lua_tostring(lua,2), lua_tostring(lua,3));

		return 0;
	}

	int dungeon_spawn_move_unique(lua_State* lua)
	{
		if (!lua_isstring(lua,1) || !lua_isnumber(lua,2) || !lua_isstring(lua,3) || !lua_isstring(lua,4))
			return 0;
		metin2::log::info("QUEST_DUNGEON_SPAWN_MOVE_UNIQUE {} {:d} {} {}", lua_tostring(lua,1), (int)lua_tonumber(lua,2), lua_tostring(lua,3), lua_tostring(lua,4));

		quest_manager& q = quest_manager::instance();
		dungeon * pDungeon = q.current_dungeon();

		if (pDungeon)
			pDungeon->spawn_move_unique(lua_tostring(lua,1), (uint32_t)lua_tonumber(lua,2), lua_tostring(lua,3), lua_tostring(lua,4));

		return 0;
	}

	int dungeon_spawn_unique(lua_State* lua)
	{
		if (!lua_isstring(lua,1) || !lua_isnumber(lua,2) || !lua_isstring(lua,3))
			return 0;
		metin2::log::info("QUEST_DUNGEON_SPAWN_UNIQUE {} {:d} {}", lua_tostring(lua,1), (int)lua_tonumber(lua,2), lua_tostring(lua,3));

		quest_manager& q = quest_manager::instance();
		dungeon * pDungeon = q.current_dungeon();

		if (pDungeon)
			pDungeon->spawn_unique(lua_tostring(lua,1), (uint32_t)lua_tonumber(lua,2), lua_tostring(lua,3));

		return 0;
	}

	int dungeon_spawn(lua_State* lua)
	{
		if (!lua_isnumber(lua,1) || !lua_isstring(lua,2))
			return 0;
		metin2::log::info("QUEST_DUNGEON_SPAWN {:d} {}", (int)lua_tonumber(lua,1), lua_tostring(lua,2));

		quest_manager& q = quest_manager::instance();
		dungeon * pDungeon = q.current_dungeon();

		if (pDungeon)
			pDungeon->spawn((uint32_t)lua_tonumber(lua,1), lua_tostring(lua,2));

		return 0;
	}

	int dungeon_set_unique(lua_State* lua)
	{
		if (!lua_isstring(lua, 1) || !lua_isnumber(lua, 2))
			return 0;

		quest_manager& q = quest_manager::instance();
		dungeon * pDungeon = q.current_dungeon();

		uint32_t vid = (uint32_t) lua_tonumber(lua, 2);

		if (pDungeon)
			pDungeon->set_unique(lua_tostring(lua, 1), vid);
		return 0;
	}

	int dungeon_get_unique_vid(lua_State* lua)
	{
		if (!lua_isstring(lua,1))
		{
			lua_pushnumber(lua,0);
			return 1;
		}

		quest_manager& q = quest_manager::instance();
		dungeon * pDungeon = q.current_dungeon();

		if (pDungeon)
		{
			lua_pushnumber(lua, pDungeon->unique_vid(lua_tostring(lua,1)));
			return 1;
		}

		lua_pushnumber(lua,0);
		return 1;
	}

	int dungeon_spawn_mob(lua_State* lua)
	{
		if (!lua_isnumber(lua, 1) || !lua_isnumber(lua, 2) || !lua_isnumber(lua, 3))
		{
			quest::error("invalid argument");
			return 0;
		}

		quest_manager& q = quest_manager::instance();
		dungeon * pDungeon = q.current_dungeon();

		uint32_t vid = 0;

		if (pDungeon)
		{
			uint32_t dwVnum = (uint32_t) lua_tonumber(lua, 1);
			int32_t x = (int32_t) lua_tonumber(lua, 2);
			int32_t y = (int32_t) lua_tonumber(lua, 3);
			float radius = lua_isnumber(lua, 4) ? (float) lua_tonumber(lua, 4) : 0;
			uint32_t count = (lua_isnumber(lua, 5)) ? (uint32_t) lua_tonumber(lua, 5) : 1;

			metin2::log::info( "dungeon_spawn_mob {:d} {:d} {:d}", dwVnum, x, y);

			if (count == 0)
				count = 1;

			while (count --)
			{
				if (radius<1)
				{
					character * ch = pDungeon->spawn_mob(dwVnum, x, y);
					if (ch && !vid)
						vid = ch->get_vid();
				}
				else
				{
					float angle = number(0, 999) * std::numbers::pi * 2 / 1000;
					float r = number(0, 999) * radius / 1000;

					int32_t nx = x + (int32_t)(r * cos(angle));
					int32_t ny = y + (int32_t)(r * sin(angle));

					character * ch = pDungeon->spawn_mob(dwVnum, nx, ny);
					if (ch && !vid)
						vid = ch->get_vid();
				}
			}
		}

		lua_pushnumber(lua, vid);
		return 1;
	}

	int dungeon_spawn_mob_dir(lua_State* lua)
	{
		if (!lua_isnumber(lua, 1) || !lua_isnumber(lua, 2) || !lua_isnumber(lua, 3) || !lua_isnumber(lua, 4))
		{
			quest::error("invalid argument");
			return 0;
		}

		quest_manager& q = quest_manager::instance();
		dungeon * pDungeon = q.current_dungeon();

		uint32_t vid = 0;

		if (pDungeon)
		{
			uint32_t dwVnum = (uint32_t) lua_tonumber(lua, 1);
			int32_t x = (int32_t) lua_tonumber(lua, 2);
			int32_t y = (int32_t) lua_tonumber(lua, 3);
			uint8_t dir = (int) lua_tonumber(lua, 4);
			
			character * ch = pDungeon->spawn_mob(dwVnum, x, y, dir);
			if (ch && !vid)
				vid = ch->get_vid();
		}
		lua_pushnumber(lua, vid);
		return 1;
	}
	
	int dungeon_spawn_mob_ac_dir(lua_State* lua)
	{
		if (!lua_isnumber(lua, 1) || !lua_isnumber(lua, 2) || !lua_isnumber(lua, 3) || !lua_isnumber(lua, 4))
		{
			quest::error("invalid argument");
			return 0;
		}

		quest_manager& q = quest_manager::instance();
		dungeon * pDungeon = q.current_dungeon();

		uint32_t vid = 0;

		if (pDungeon)
		{
			uint32_t dwVnum = (uint32_t) lua_tonumber(lua, 1);
			int32_t x = (int32_t) lua_tonumber(lua, 2);
			int32_t y = (int32_t) lua_tonumber(lua, 3);
			uint8_t dir = (int) lua_tonumber(lua, 4);
			
			character * ch = pDungeon->spawn_mob_ac_dir(dwVnum, x, y, dir);
			if (ch && !vid)
				vid = ch->get_vid();
		}
		lua_pushnumber(lua, vid);
		return 1;
	}

	int dungeon_spawn_goto_mob(lua_State* lua)
	{
		if (!lua_isnumber(lua, 1) || !lua_isnumber(lua, 2) || !lua_isnumber(lua, 3) || !lua_isnumber(lua, 4))
			return 0;

		int32_t lFromX = (int32_t)lua_tonumber(lua, 1);
		int32_t lFromY = (int32_t)lua_tonumber(lua, 2);
		int32_t lToX   = (int32_t)lua_tonumber(lua, 3);
		int32_t lToY   = (int32_t)lua_tonumber(lua, 4);

		quest_manager& q = quest_manager::instance();
		dungeon * pDungeon = q.current_dungeon();

		if (pDungeon)
			pDungeon->spawn_goto_mob(lFromX, lFromY, lToX, lToY);

		return 0;
	}

	int dungeon_spawn_name_mob(lua_State* lua)
	{
		if (!lua_isnumber(lua, 1) || !lua_isnumber(lua, 2) || !lua_isnumber(lua, 3) || !lua_isstring(lua, 4))
			return 0;

		quest_manager& q = quest_manager::instance();
		dungeon * pDungeon = q.current_dungeon();

		if (pDungeon)
		{
			uint32_t dwVnum = (uint32_t) lua_tonumber(lua, 1);
			int32_t x = (int32_t)lua_tonumber(lua, 2);
			int32_t y = (int32_t)lua_tonumber(lua, 3);
			pDungeon->spawn_name_mob(dwVnum, x, y, lua_tostring(lua, 4));
		}
		return 0;
	}

	int dungeon_spawn_group(lua_State* lua)
	{
		//
		// argument: vnum,x,y,radius,aggressive,count
		//
		if (!lua_isnumber(lua, 1) || !lua_isnumber(lua, 2) || !lua_isnumber(lua, 3) || !lua_isnumber(lua, 4) || !lua_isnumber(lua, 6))
		{
			quest::error("invalid argument");
			return 0;
		}

		uint32_t vid = 0;

		quest_manager& q = quest_manager::instance();
		dungeon * pDungeon = q.current_dungeon();

		if (pDungeon)
		{
			uint32_t group_vnum = (uint32_t)lua_tonumber(lua, 1);
			int32_t local_x = (int32_t) lua_tonumber(lua, 2) * 100;
			int32_t local_y = (int32_t) lua_tonumber(lua, 3) * 100;
			float radius = (float) lua_tonumber(lua, 4) * 100;
			bool bAggressive = lua_toboolean(lua, 5);
			uint32_t count = (uint32_t) lua_tonumber(lua, 6);

			character * chRet = pDungeon->spawn_group(group_vnum, local_x, local_y, radius, bAggressive, count);
			if (chRet)
				vid = chRet->get_vid();
		}

		lua_pushnumber(lua, vid);
		return 1;
	}

	int dungeon_join(lua_State* lua)
	{
		if (lua_gettop(lua) < 1 || !lua_isnumber(lua, 1))
			return 0;

		int32_t lMapIndex = (int32_t)lua_tonumber(lua, 1);
		dungeon * pDungeon = dungeon_manager::instance().create(lMapIndex);

		if (!pDungeon)
			return 0;

		character * ch = quest_manager::instance().current_character_ptr();

		if (ch->get_party() && ch->get_party()->leader_pid() == ch->player_id())
			pDungeon->join_party(ch->get_party());
		else if (!ch->get_party())
			pDungeon->join(ch);

		return 0;
	}

	int dungeon_exit(lua_State* lua) // send to the position before entering the dungeon
	{
		character * ch = quest_manager::instance().current_character_ptr();

		ch->exit_to_saved_location();
		return 0;
	}

	int dungeon_exit_all(lua_State* lua) // send everyone in the dungeon to their position before entering the dungeon
	{
		quest_manager& q = quest_manager::instance();
		dungeon * pDungeon = q.current_dungeon();

		if (pDungeon)
			pDungeon->exit_all();

		return 0;
	}

	struct f_say_dungeon_by_item_group
	{
		const dungeon::item_group* item_group;
		std::string can_enter_ment;
		std::string cant_enter_ment;
		void operator()(entity * ent)
		{
			if (ent->is_type(ENTITY_CHARACTER))
			{
				character * ch = (character *) ent;

				if (ch->is_pc())
				{
					struct metin2::game::packet_script packet_script;
					temp_buffer buf;
					
					for (dungeon::item_group::const_iterator it = item_group->begin(); it != item_group->end(); it++)
					{
						if(ch->count_specify_item(it->first) >= it->second)
						{
							packet_script.header = HEADER_GC_SCRIPT;
							packet_script.skin = quest::quest_manager::QUEST_SKIN_NORMAL;
							packet_script.src_size = can_enter_ment.size();
							packet_script.size = packet_script.src_size + sizeof(struct game::protocol::packet_script);

							buf.write(&packet_script, sizeof(struct game::protocol::packet_script));
							buf.write(&can_enter_ment[0], can_enter_ment.size());
							ch->get_desc()->packet(buf.read_peek(), buf.size());
							return;
						}
					}

					packet_script.header = HEADER_GC_SCRIPT;
					packet_script.skin = quest::quest_manager::QUEST_SKIN_NORMAL;
					packet_script.src_size = cant_enter_ment.size();
					packet_script.size = packet_script.src_size + sizeof(struct game::protocol::packet_script);

					buf.write(&packet_script, sizeof(struct game::protocol::packet_script));
					buf.write(&cant_enter_ment[0], cant_enter_ment.size());
					ch->get_desc()->packet(buf.read_peek(), buf.size());
				}
			}
		}
	};


	int dungeon_say_diff_by_item_group(lua_State* lua)
	{
		if (!lua_isstring(lua, 1) || !lua_isstring(lua, 2) || !lua_isstring(lua, 3))
		{
			metin2::log::info( "QUEST wrong set flag");
			return 0;
		}

		quest_manager& q = quest_manager::instance();
		dungeon * pDungeon = q.current_dungeon();

		if(!pDungeon)
		{
			quest::error("QUEST : no dungeon");
			return 0;
		}

		sectree_map * pMap = sectree_manager::instance().map(pDungeon->map_index());

		if (!pMap)
		{
			quest::error("cannot find map by index {:d}", pDungeon->map_index());
			return 0;
		}
		f_say_dungeon_by_item_group f;
		metin2::log::info("diff_by_item");
	
		std::string group_name (lua_tostring (lua, 1));
		f.item_group = pDungeon->get_item_group (group_name);

		if (f.item_group == nullptr)
		{
			quest::error("invalid item group");
			return 0;
		}

		f.can_enter_ment = lua_tostring(lua, 2);
		f.can_enter_ment+= "[ENTER][ENTER][ENTER][ENTER][DONE]";
		f.cant_enter_ment = lua_tostring(lua, 3);
		f.cant_enter_ment+= "[ENTER][ENTER][ENTER][ENTER][DONE]";

		pMap -> for_each( f );

		return 0;
	}
	
	struct f_exit_dungeon_by_item_group
	{
		const dungeon::item_group* item_group;
		
		void operator()(entity * ent)
		{
			if (ent->is_type(ENTITY_CHARACTER))
			{
				character * ch = (character *) ent;

				if (ch->is_pc())
				{
					for (dungeon::item_group::const_iterator it = item_group->begin(); it != item_group->end(); it++)
					{
						if(ch->count_specify_item(it->first) >= it->second)
						{
							return;
						}
					}
					ch->exit_to_saved_location();
				}
			}
		}
	};
	
	int dungeon_exit_all_by_item_group (lua_State* lua) // kick anyone who doesn't have an item from a specific item group
	{
		if (!lua_isstring(lua, 1))
		{
			metin2::log::info( "QUEST wrong set flag");
			return 0;
		}

		quest_manager& q = quest_manager::instance();
		dungeon * pDungeon = q.current_dungeon();

		if(!pDungeon)
		{
			quest::error("QUEST : no dungeon");
			return 0;
		}

		sectree_map * pMap = sectree_manager::instance().map(pDungeon->map_index());

		if (!pMap)
		{
			quest::error("cannot find map by index {:d}", pDungeon->map_index());
			return 0;
		}
		f_exit_dungeon_by_item_group f;

		std::string group_name (lua_tostring (lua, 1));
		f.item_group = pDungeon->get_item_group (group_name);
	
		if (f.item_group == nullptr)
		{
			quest::error("invalid item group");
			return 0;
		}
		
		pMap -> for_each( f );
		
		return 0;
	}

	struct f_delete_item_in_item_group
	{
		const dungeon::item_group* item_group;

		void operator()(entity * ent)
		{
			if (ent->is_type(ENTITY_CHARACTER))
			{
				character * ch = (character *) ent;

				if (ch->is_pc())
				{
					for (dungeon::item_group::const_iterator it = item_group->begin(); it != item_group->end(); it++)
					{
						if(ch->count_specify_item(it->first) >= it->second)
						{
							ch->remove_specify_item (it->first, it->second);
							return;
						}
					}
				}
			}
		}
	};
	
	int dungeon_delete_item_in_item_group_from_all(lua_State* lua) // remove a specific item from PCs in the dungeon.
	{
		if (!lua_isstring(lua, 1))
		{
			metin2::log::info( "QUEST wrong set flag");
			return 0;
		}

		quest_manager& q = quest_manager::instance();
		dungeon * pDungeon = q.current_dungeon();

		if(!pDungeon)
		{
			quest::error("QUEST : no dungeon");
			return 0;
		}

		sectree_map * pMap = sectree_manager::instance().map(pDungeon->map_index());

		if (!pMap)
		{
			quest::error("cannot find map by index {:d}", pDungeon->map_index());
			return 0;
		}
		f_delete_item_in_item_group f;

		std::string group_name (lua_tostring (lua, 1));
		f.item_group = pDungeon->get_item_group (group_name);

		if (f.item_group == nullptr)
		{
			quest::error("invalid item group");
			return 0;
		}

		pMap -> for_each( f );
		
		return 0;
	}


	int dungeon_kill_all(lua_State* lua)
	{
		quest_manager& q = quest_manager::instance();
		dungeon * pDungeon = q.current_dungeon();

		if (pDungeon)
			pDungeon->kill_all();

		return 0;
	}

	int dungeon_purge(lua_State* lua)
	{
		quest_manager& q = quest_manager::instance();
		dungeon * pDungeon = q.current_dungeon();

		if (pDungeon)
			pDungeon->purge();

		return 0;
	}

	int dungeon_exit_all_to_start_position(lua_State * lua)
	{
		quest_manager& q = quest_manager::instance();
		dungeon * pDungeon = q.current_dungeon();

		if (pDungeon)
			pDungeon->exit_all_to_start_position();

		return 0;
	}

	int dungeon_count_monster(lua_State * lua)
	{
		quest_manager& q = quest_manager::instance();
		dungeon * pDungeon = q.current_dungeon();

		if (pDungeon)
			lua_pushnumber(lua, pDungeon->count_monster());
		else
		{
			quest::error("not in a dungeon");
			lua_pushnumber(lua, INT32_MAX);
		}

		return 1;
	}

	int dungeon_select(lua_State* lua)
	{
		uint32_t dwMapIndex = (uint32_t) lua_tonumber(lua, 1);
		if (dwMapIndex)
		{
			dungeon * pDungeon = dungeon_manager::instance().find_by_map_index(dwMapIndex);
			if (pDungeon)
			{
				quest_manager::instance().select_dungeon(pDungeon);
				lua_pushboolean(lua, 1);
			}
			else
			{
				quest_manager::instance().select_dungeon(nullptr);
				lua_pushboolean(lua, 0);
			}
		}
		else
		{
			quest_manager::instance().select_dungeon(nullptr);
			lua_pushboolean(lua, 0);
		}
		return 1;
	}

	int dungeon_find(lua_State* lua)
	{
		uint32_t dwMapIndex = (uint32_t) lua_tonumber(lua, 1);
		if (dwMapIndex)
		{
			dungeon * pDungeon = dungeon_manager::instance().find_by_map_index(dwMapIndex);
			if (pDungeon)
			{
				lua_pushboolean(lua, 1);
			}
			else
			{
				lua_pushboolean(lua, 0);
			}
		}
		else
		{
			lua_pushboolean(lua, 0);
		}
		return 1;
	}

	int dungeon_all_near_to( lua_State* lua)
	{
		dungeon * pDungeon = quest_manager::instance().current_dungeon();

		if (pDungeon != nullptr)
		{
			lua_pushboolean(lua, pDungeon->is_all_pc_near_to( (int)lua_tonumber(lua, 1), (int)lua_tonumber(lua, 2), 30));
		}
		else
		{
			lua_pushboolean(lua, false);
		}

		return 1;
	}

	int dungeon_set_warp_location (lua_State* lua)
	{
		dungeon * pDungeon = quest_manager::instance().current_dungeon();

		if (pDungeon == nullptr)
		{
			return 0;
		}
		
		if (lua_gettop(lua)<3 || !lua_isnumber(lua, 1) || !lua_isnumber(lua,2) || !lua_isnumber(lua, 3))
		{
			return 0;
		}

		f_set_warp_location f ((int)lua_tonumber(lua, 1), (int)lua_tonumber(lua, 2), (int)lua_tonumber(lua, 3));
		pDungeon->for_each_member (f);

		return 0;
	}

	int dungeon_set_item_group (lua_State* lua)
	{
		if (!lua_isstring (lua, 1) || !lua_isnumber (lua, 2))
		{
			return 0;
		}
		std::string group_name (lua_tostring (lua, 1));
		int size = lua_tonumber (lua, 2);

		dungeon::item_group item_group;
		
		for (int i = 0; i < size; i++)
		{
			if (!lua_isnumber (lua, i * 2 + 3) || !lua_isnumber (lua, i * 2 + 4))
			{
				return 0;
			}
			item_group.push_back (std::pair <uint32_t, int> (lua_tonumber (lua, i * 2 + 3), lua_tonumber (lua, i * 2 + 4)));
		}
		dungeon * pDungeon = quest_manager::instance().current_dungeon();

		if (pDungeon == nullptr)
		{
			return 0;
		}
		
		pDungeon->create_item_group (group_name, item_group);
		return 0;
	}

	int dungeon_set_quest_flag2(lua_State* lua)
	{
		quest_manager & q = quest_manager::instance();

		f_set_quest_flag f;

		if (!lua_isstring(lua, 1) || !lua_isstring(lua, 2) || !lua_isnumber(lua, 3))
		{
			quest::error("Invalid Argument");
		}

		f.flagname = std::string (lua_tostring(lua, 1)) + "." + lua_tostring(lua, 2);
		f.value = (int) rint(lua_tonumber(lua, 3));

		dungeon * pDungeon = q.current_dungeon();

		if (pDungeon)
			pDungeon->for_each_member(f);

		return 0;
	}

	void register_dungeon_function_table() 
	{
		luaL_reg dungeon_functions[] = 
		{
			{ "join",			dungeon_join		},
			{ "exit",			dungeon_exit		},
			{ "exit_all",		dungeon_exit_all	},
			{ "set_item_group",	dungeon_set_item_group	},
			{ "exit_all_by_item_group",	dungeon_exit_all_by_item_group},
			{ "say_diff_by_item_group",	dungeon_say_diff_by_item_group},
			{ "delete_item_in_item_group_from_all", dungeon_delete_item_in_item_group_from_all},
			{ "purge",			dungeon_purge		},
			{ "kill_all",		dungeon_kill_all	},
			{ "spawn",			dungeon_spawn		},
			{ "spawn_mob",		dungeon_spawn_mob	},
			{ "spawn_mob_dir",	dungeon_spawn_mob_dir	},
			{ "spawn_mob_ac_dir",	dungeon_spawn_mob_ac_dir	},
			{ "spawn_name_mob",	dungeon_spawn_name_mob	},
			{ "spawn_goto_mob",		dungeon_spawn_goto_mob	},
			{ "spawn_group",		dungeon_spawn_group	},
			{ "spawn_unique",		dungeon_spawn_unique	},
			{ "spawn_move_unique",		dungeon_spawn_move_unique},
			{ "spawn_move_group",		dungeon_spawn_move_group},
			{ "spawn_stone_door",		dungeon_spawn_stone_door},
			{ "spawn_wooden_door",		dungeon_spawn_wooden_door},
			{ "purge_unique",		dungeon_purge_unique	},
			{ "purge_area",			dungeon_purge_area	},
			{ "kill_unique",		dungeon_kill_unique	},
			{ "is_unique_dead",		dungeon_is_unique_dead	},
			{ "unique_get_hp_perc",		dungeon_unique_get_hp_perc},
			{ "unique_set_def_grade",	dungeon_unique_set_def_grade},
			{ "unique_set_hp",		dungeon_unique_set_hp	},
			{ "unique_set_maxhp",		dungeon_unique_set_maxhp},
			{ "get_unique_vid",		dungeon_get_unique_vid},
			{ "get_kill_stone_count",	dungeon_get_kill_stone_count},
			{ "get_kill_mob_count",		dungeon_get_kill_mob_count},
			{ "is_use_potion",		dungeon_is_use_potion	},
			{ "revived",			dungeon_revived		},
			{ "set_dest",			dungeon_set_dest	},
			{ "jump_all",			dungeon_jump_all	},
			{ "warp_all",		dungeon_warp_all	},
			{ "new_jump_all",		dungeon_new_jump_all	},
			{ "new_jump_party",		dungeon_new_jump_party	},
			{ "new_jump",			dungeon_new_jump	},
			{ "regen_file",			dungeon_regen_file	},
			{ "set_regen_file",		dungeon_set_regen_file	},
			{ "clear_regen",		dungeon_clear_regen	},
			{ "set_exit_all_at_eliminate",	dungeon_set_exit_all_at_eliminate},
			{ "set_warp_at_eliminate",	dungeon_set_warp_at_eliminate},
			{ "get_map_index",		dungeon_get_map_index	},
			{ "check_eliminated",		dungeon_check_eliminated},
			{ "exit_all_to_start_position",	dungeon_exit_all_to_start_position },
			{ "count_monster",		dungeon_count_monster	},
			{ "setf",					dungeon_set_flag	},
			{ "getf",					dungeon_get_flag	},
			{ "getf_from_map_index",	dungeon_get_flag_from_map_index	},
			{ "set_unique",			dungeon_set_unique	},
			{ "select",			dungeon_select		},
			{ "find",			dungeon_find		},
			{ "notice",			dungeon_notice		},
			{ "setqf",			dungeon_set_quest_flag	},
			{ "all_near_to",	dungeon_all_near_to	},
			{ "set_warp_location",	dungeon_set_warp_location	},
			{ "setqf2",			dungeon_set_quest_flag2	},

			{ nullptr,				nullptr			}
		};

		quest_manager::instance().add_lua_function_table("d", dungeon_functions);
	}
}
}
