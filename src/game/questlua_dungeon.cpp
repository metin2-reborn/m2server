#include "buffer_manager.h"
#include "char.h"
#include "char_manager.h"
#include "desc.h"
#include "dungeon.h"
#include "lua_incl.h"
#include "packet.h"
#include "party.h"
#include "questlua.h"
#include "questmanager.h"
#include "sectree_manager.h"
#include "typedef.h"

#include <metin2/log/log.h>
#include <libthecore/utils.h>

#include <cmath>
#include <cstdint>
#include <numbers>
#include <string>
#include <utility>

namespace metin2::game {



template <class Func> Func CDungeon::ForEachMember(Func f)
{
	decltype(m_set_pkCharacter.begin()) it;

	for (it = m_set_pkCharacter.begin(); it != m_set_pkCharacter.end(); ++it)
	{
		metin2::log::info( "Dungeon ForEachMember {}", (*it)->GetName());
		f(*it);
	}
	return f;
}

namespace quest
{
	//
	// "dungeon" lua functions
	//
	int dungeon_notice(lua_State* L)
	{
		if (!lua_isstring(L, 1))
			return 0;

		CQuestManager& q = CQuestManager::instance();
		CDungeon * pDungeon = q.GetCurrentDungeon();

		if (pDungeon)
			pDungeon->Notice(lua_tostring(L, 1));
		return 0;
	}

	int dungeon_set_quest_flag(lua_State* L)
	{
		CQuestManager & q = CQuestManager::instance();

		FSetQuestFlag f;

		f.flagname = q.GetCurrentPC()->GetCurrentQuestName() + "." + lua_tostring(L, 1);
		f.value = (int) rint(lua_tonumber(L, 2));

		CDungeon * pDungeon = q.GetCurrentDungeon();

		if (pDungeon)
			pDungeon->ForEachMember(f);

		return 0;
	}

	int dungeon_set_flag(lua_State* L)
	{
		if (!lua_isstring(L,1) || !lua_isnumber(L,2))
		{
			quest::error("wrong set flag");
		}
		else
		{
			CQuestManager& q = CQuestManager::instance();
			CDungeon * pDungeon = q.GetCurrentDungeon();

			if (pDungeon)
			{
				const char* sz = lua_tostring(L,1);
				int value = int(lua_tonumber(L, 2));
				pDungeon->SetFlag(sz, value);
			}
			else
			{
				quest::error("no dungeon !!!");
			}
		}
		return 0;
	}

	int dungeon_get_flag(lua_State* L)
	{
		if (!lua_isstring(L,1))
		{
			quest::error("wrong get flag");
		}

		CQuestManager& q = CQuestManager::instance();
		CDungeon * pDungeon = q.GetCurrentDungeon();

		if (pDungeon)
		{
			const char* sz = lua_tostring(L,1);
			lua_pushnumber(L, pDungeon->GetFlag(sz));
		}
		else
		{
			quest::error("no dungeon !!!");
			lua_pushnumber(L, 0);
		}

		return 1;
	}

	int dungeon_get_flag_from_map_index(lua_State* L)
	{
		if (!lua_isstring(L,1) || !lua_isnumber(L,2))
		{
			quest::error("wrong get flag");
		}

		uint32_t dwMapIndex = (uint32_t) lua_tonumber(L, 2);
		if (dwMapIndex)
		{
			CDungeon * pDungeon = CDungeonManager::instance().FindByMapIndex(dwMapIndex);
			if (pDungeon)
			{
				const char* sz = lua_tostring(L,1);
				lua_pushnumber(L, pDungeon->GetFlag(sz));
			}
			else
			{
				quest::error("no dungeon !!!");
				lua_pushnumber(L, 0);
			}
		}
		else
		{
			lua_pushboolean(L, 0);
		}
		return 1;
	}

	int dungeon_get_map_index(lua_State* L)
	{
		CQuestManager& q = CQuestManager::instance();
		CDungeon * pDungeon = q.GetCurrentDungeon();

		if (pDungeon)
		{
			metin2::log::info( "Dungeon GetMapIndex {:d}",pDungeon->GetMapIndex());
			lua_pushnumber(L, pDungeon->GetMapIndex());
		}
		else
		{
			quest::error("no dungeon !!!");
			lua_pushnumber(L, 0);
		}

		return 1;
	}

	int dungeon_regen_file(lua_State* L)
	{
		if (!lua_isstring(L,1))
		{
			quest::error("wrong filename");
			return 0;
		}

		CQuestManager& q = CQuestManager::instance();
		CDungeon * pDungeon = q.GetCurrentDungeon();

		if (pDungeon)
			pDungeon->SpawnRegen(lua_tostring(L,1));

		return 0;
	}

	int dungeon_set_regen_file(lua_State* L)
	{
		if (!lua_isstring(L,1))
		{
			quest::error("wrong filename");
			return 0;
		}
		CQuestManager& q = CQuestManager::instance();
		CDungeon * pDungeon = q.GetCurrentDungeon();

		if (pDungeon)
			pDungeon->SpawnRegen(lua_tostring(L,1), false);
		return 0;
	}

	int dungeon_clear_regen(lua_State* L)
	{
		CQuestManager& q = CQuestManager::instance();
		CDungeon * pDungeon = q.GetCurrentDungeon();
		if (pDungeon)
			pDungeon->ClearRegen();
		return 0;
	}

	int dungeon_check_eliminated(lua_State* L)
	{
		CQuestManager& q = CQuestManager::instance();
		CDungeon * pDungeon = q.GetCurrentDungeon();
		if (pDungeon)
			pDungeon->CheckEliminated();
		return 0;
	}

	int dungeon_set_exit_all_at_eliminate(lua_State* L)
	{
		if (!lua_isnumber(L,1))
		{
			quest::error("wrong time");
			return 0;
		}

		CQuestManager& q = CQuestManager::instance();
		CDungeon * pDungeon = q.GetCurrentDungeon();

		if (pDungeon)
			pDungeon->SetExitAllAtEliminate((int32_t)lua_tonumber(L, 1));

		return 0;
	}

	int dungeon_set_warp_at_eliminate(lua_State* L)
	{
		if (!lua_isnumber(L, 1))
		{
			quest::error("wrong time");
			return 0;
		}

		if (!lua_isnumber(L, 2))
		{
			quest::error("wrong map index");
			return 0;
		}

		if (!lua_isnumber(L, 3)) 
		{
			quest::error("wrong X");
			return 0;
		}

		if (!lua_isnumber(L, 4))
		{
			quest::error("wrong Y");
			return 0;
		}

		const char * c_pszRegenFile = nullptr;

		if (lua_gettop(L) >= 5)
			c_pszRegenFile = lua_tostring(L,5);

		CQuestManager& q = CQuestManager::instance();
		CDungeon * pDungeon = q.GetCurrentDungeon();

		if (pDungeon)
		{
			pDungeon->SetWarpAtEliminate((int32_t)lua_tonumber(L,1),
										 (int32_t)lua_tonumber(L,2),
										 (int32_t)lua_tonumber(L,3),
										 (int32_t)lua_tonumber(L,4),
										 c_pszRegenFile);
		}
		else
			quest::error("cannot find dungeon");

		return 0;
	}

	int dungeon_new_jump(lua_State* L)
	{
		if (lua_gettop(L) < 3)
		{
			quest::error("not enough argument");
			return 0;
		}

		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3))
		{
			quest::error("wrong argument");
			return 0;
		}

		int32_t lMapIndex = (int32_t)lua_tonumber(L,1);

		CDungeon * pDungeon = CDungeonManager::instance().Create(lMapIndex);

		if (!pDungeon)
		{
			quest::error("cannot create dungeon {:d}", lMapIndex);
			return 0;
		}

		CHARACTER * ch = CQuestManager::instance().GetCurrentCharacterPtr();

		ch->WarpSet((int) lua_tonumber(L, 2), (int)lua_tonumber(L, 3), pDungeon->GetMapIndex());
		return 0;
	}

	int dungeon_new_jump_all(lua_State* L)
	{
		if (lua_gettop(L)<3 || !lua_isnumber(L,1) || !lua_isnumber(L, 2) || !lua_isnumber(L,3))
		{
			quest::error("not enough argument");
			return 0;
		}

		int32_t lMapIndex = (int32_t)lua_tonumber(L,1);

		CDungeon * pDungeon = CDungeonManager::instance().Create(lMapIndex);

		if (!pDungeon)
		{
			quest::error("cannot create dungeon {:d}", lMapIndex);
			return 0;
		}

		CHARACTER * ch = CQuestManager::instance().GetCurrentCharacterPtr();

		pDungeon->JumpAll(ch->GetMapIndex(), (int)lua_tonumber(L, 2), (int)lua_tonumber(L, 3));

		return 0;
	}

	int dungeon_new_jump_party (lua_State* L)
	{
		if (lua_gettop(L)<3 || !lua_isnumber(L,1) || !lua_isnumber(L, 2) || !lua_isnumber(L,3))
		{
			quest::error("not enough argument");
			return 0;
		}

		int32_t lMapIndex = (int32_t)lua_tonumber(L,1);

		CDungeon * pDungeon = CDungeonManager::instance().Create(lMapIndex);

		if (!pDungeon)
		{
			quest::error("cannot create dungeon {:d}", lMapIndex);
			return 0;
		}

		CHARACTER * ch = CQuestManager::instance().GetCurrentCharacterPtr();

		if (ch->GetParty() == nullptr)
		{
			quest::error("cannot go to dungeon alone.");
			return 0;
		}
		pDungeon->JumpParty(ch->GetParty(), ch->GetMapIndex(), (int)lua_tonumber(L, 2), (int)lua_tonumber(L, 3));

		return 0;
	}

	int dungeon_jump_all(lua_State* L)
	{
		if (lua_gettop(L)<2 || !lua_isnumber(L, 1) || !lua_isnumber(L,2))
			return 0;

		CDungeon * pDungeon = CQuestManager::instance().GetCurrentDungeon();

		if (!pDungeon)
			return 0;

		pDungeon->JumpAll(pDungeon->GetMapIndex(), (int)lua_tonumber(L, 1), (int)lua_tonumber(L, 2));
		return 0;
	}

	int dungeon_warp_all(lua_State* L)
	{
		if (lua_gettop(L)<2 || !lua_isnumber(L, 1) || !lua_isnumber(L,2))
			return 0;

		CDungeon * pDungeon = CQuestManager::instance().GetCurrentDungeon();

		if (!pDungeon)
			return 0;

		pDungeon->WarpAll(pDungeon->GetMapIndex(), (int)lua_tonumber(L, 1), (int)lua_tonumber(L, 2));
		return 0;
	}

	int dungeon_get_kill_stone_count(lua_State* L)
	{
		if (!lua_isnumber(L,1) || !lua_isnumber(L,2))
		{
			lua_pushnumber(L, 0);
			return 1;
		}


		CQuestManager& q = CQuestManager::instance();
		CDungeon * pDungeon = q.GetCurrentDungeon();

		if (pDungeon)
		{
			lua_pushnumber(L, pDungeon->GetKillStoneCount());
			return 1;
		}

		lua_pushnumber(L, 0);
		return 1;
	}

	int dungeon_get_kill_mob_count(lua_State * L)
	{
		if (!lua_isnumber(L,1) || !lua_isnumber(L,2))
		{
			lua_pushnumber(L, 0);
			return 1;
		}

		CQuestManager& q = CQuestManager::instance();
		CDungeon * pDungeon = q.GetCurrentDungeon();

		if (pDungeon)
		{
			lua_pushnumber(L, pDungeon->GetKillMobCount());
			return 1;
		}

		lua_pushnumber(L, 0);
		return 1;
	}

	int dungeon_is_use_potion(lua_State* L)
	{
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2))
		{
			lua_pushboolean(L, 1);
			return 1;
		}

		CQuestManager& q = CQuestManager::instance();
		CDungeon * pDungeon = q.GetCurrentDungeon();

		if (pDungeon)
		{
			lua_pushboolean(L, pDungeon->IsUsePotion());
			return 1;
		}

		lua_pushboolean(L, 1);
		return 1;
	}

	int dungeon_revived(lua_State* L) 
	{
		if (!lua_isnumber(L,1) || !lua_isnumber(L,2))
		{
			lua_pushboolean(L, 1);
			return 1;
		}

		CQuestManager& q = CQuestManager::instance();
		CDungeon * pDungeon = q.GetCurrentDungeon();

		if (pDungeon)
		{
			lua_pushboolean(L, pDungeon->IsUseRevive());
			return 1;
		}

		lua_pushboolean(L, 1);
		return 1;
	}

	int dungeon_set_dest(lua_State* L)
	{
		if (!lua_isnumber(L,1) || !lua_isnumber(L,2))
			return 0;

		CHARACTER * ch = CQuestManager::instance().GetCurrentCharacterPtr();

		CParty * pParty = ch->GetParty();
		CQuestManager& q = CQuestManager::instance();
		CDungeon * pDungeon = q.GetCurrentDungeon();

		if (pDungeon && pParty)
			pDungeon->SendDestPositionToParty(pParty, (int)lua_tonumber(L,1), (int)lua_tonumber(L,2));

		return 0;
	}

	int dungeon_unique_set_maxhp(lua_State* L)
	{
		if (!lua_isstring(L,1) || !lua_isnumber(L,2))
			return 0;

		CQuestManager& q = CQuestManager::instance();
		CDungeon * pDungeon = q.GetCurrentDungeon();

		if (pDungeon)
			pDungeon->UniqueSetMaxHP(lua_tostring(L,1), (int)lua_tonumber(L,2));

		return 0;
	}

	int dungeon_unique_set_hp(lua_State* L)
	{
		if (!lua_isstring(L,1) || !lua_isnumber(L,2))
			return 0;

		CQuestManager& q = CQuestManager::instance();
		CDungeon * pDungeon = q.GetCurrentDungeon();

		if (pDungeon)
			pDungeon->UniqueSetHP(lua_tostring(L,1), (int)lua_tonumber(L,2));

		return 0;
	}

	int dungeon_unique_set_def_grade(lua_State* L)
	{
		if (!lua_isstring(L,1) || !lua_isnumber(L,2))
			return 0;

		CQuestManager& q = CQuestManager::instance();
		CDungeon * pDungeon = q.GetCurrentDungeon();

		if (pDungeon)
			pDungeon->UniqueSetDefGrade(lua_tostring(L,1), (int)lua_tonumber(L,2));

		return 0;
	}

	int dungeon_unique_get_hp_perc(lua_State* L)
	{
		if (!lua_isstring(L,1))
		{
			lua_pushnumber(L,0);
			return 1;
		}

		CQuestManager& q = CQuestManager::instance();
		CDungeon * pDungeon = q.GetCurrentDungeon();

		if (pDungeon)
		{
			lua_pushnumber(L, pDungeon->GetUniqueHpPerc(lua_tostring(L,1)));
			return 1;
		}

		lua_pushnumber(L,0);
		return 1;
	}

	int dungeon_is_unique_dead(lua_State* L)
	{
		if (!lua_isstring(L,1))
		{
			lua_pushboolean(L, 0);
			return 1;
		}

		CQuestManager& q = CQuestManager::instance();
		CDungeon * pDungeon = q.GetCurrentDungeon();

		if (pDungeon)
		{
			lua_pushboolean(L, pDungeon->IsUniqueDead(lua_tostring(L,1))?1:0);
			return 1;
		}

		lua_pushboolean(L, 0);
		return 1;
	}

	int dungeon_purge_unique(lua_State* L)
	{
		if (!lua_isstring(L,1))
			return 0;
		metin2::log::info("QUEST_DUNGEON_PURGE_UNIQUE {}", lua_tostring(L,1));

		CQuestManager& q = CQuestManager::instance();
		CDungeon * pDungeon = q.GetCurrentDungeon();

		if (pDungeon)
			pDungeon->PurgeUnique(lua_tostring(L,1));

		return 0;
	}

	struct FPurgeArea
	{
		int x1, y1, x2, y2;
		CHARACTER * ExceptChar;

		FPurgeArea(int a, int b, int c, int d, CHARACTER * p)
			: x1(a), y1(b), x2(c), y2(d),
			ExceptChar(p)
		{}

		void operator () (CEntity * ent)
		{
			if (true == ent->IsType(ENTITY_CHARACTER))
			{
				CHARACTER * pChar = static_cast<CHARACTER *>(ent);

				if (pChar == ExceptChar)
					return;
					
				if (!pChar->IsPet() && (true == pChar->IsMonster() || true == pChar->IsStone()))
				{
					if (x1 <= pChar->GetX() && pChar->GetX() <= x2 && y1 <= pChar->GetY() && pChar->GetY() <= y2)
					{
						CHARACTER_MANAGER::instance().DestroyCharacter(pChar);
					}
				}
			}
		}
	};
	
	int dungeon_purge_area(lua_State* L)
	{
		if (!lua_isnumber(L,1) || !lua_isnumber(L,2) || !lua_isnumber(L,3) || !lua_isnumber(L,4))
			return 0;
		metin2::log::info("QUEST_DUNGEON_PURGE_AREA");

		int x1 = lua_tonumber(L, 1);
		int y1 = lua_tonumber(L, 2);
		int x2 = lua_tonumber(L, 3);
		int y2 = lua_tonumber(L, 4);

		CQuestManager& q = CQuestManager::instance();
		CDungeon * pDungeon = q.GetCurrentDungeon();

		const int mapIndex = pDungeon->GetMapIndex();

		if (0 == mapIndex)
		{
			quest::error("_purge_area: cannot get a map index with ({:d}, {:d})", x1, y1);
			return 0;
		}

		SECTREE_MAP * pSectree = SECTREE_MANAGER::instance().GetMap(mapIndex);

		if (nullptr != pSectree)
		{
			FPurgeArea func(x1, y1, x2, y2, CQuestManager::instance().GetCurrentNPCCharacterPtr());

			pSectree->for_each(func);
		}

		return 0;
	}

	int dungeon_kill_unique(lua_State* L)
	{
		if (!lua_isstring(L,1))
			return 0;
		metin2::log::info("QUEST_DUNGEON_KILL_UNIQUE {}", lua_tostring(L,1));

		CQuestManager& q = CQuestManager::instance();
		CDungeon * pDungeon = q.GetCurrentDungeon();

		if (pDungeon)
			pDungeon->KillUnique(lua_tostring(L,1));

		return 0;
	}

	int dungeon_spawn_stone_door(lua_State* L)
	{
		if (!lua_isstring(L,1) || !lua_isstring(L,2))
			return 0;
		metin2::log::info("QUEST_DUNGEON_SPAWN_STONE_DOOR {} {}", lua_tostring(L,1), lua_tostring(L,2));

		CQuestManager& q = CQuestManager::instance();
		CDungeon * pDungeon = q.GetCurrentDungeon();

		if (pDungeon)
			pDungeon->SpawnStoneDoor(lua_tostring(L,1), lua_tostring(L,2));

		return 0;
	}

	int dungeon_spawn_wooden_door(lua_State* L)
	{
		if (!lua_isstring(L,1) || !lua_isstring(L,2))
			return 0;
		metin2::log::info("QUEST_DUNGEON_SPAWN_WOODEN_DOOR {} {}", lua_tostring(L,1), lua_tostring(L,2));

		CQuestManager& q = CQuestManager::instance();
		CDungeon * pDungeon = q.GetCurrentDungeon();

		if (pDungeon)
			pDungeon->SpawnWoodenDoor(lua_tostring(L,1), lua_tostring(L,2));

		return 0;
	}

	int dungeon_spawn_move_group(lua_State* L)
	{
		if (!lua_isnumber(L,1) || !lua_isstring(L,2) || !lua_isstring(L,3))
			return 0;
		metin2::log::info("QUEST_DUNGEON_SPAWN_MOVE_GROUP {:d} {} {}", (int)lua_tonumber(L,1), lua_tostring(L,2), lua_tostring(L,3));

		CQuestManager& q = CQuestManager::instance();
		CDungeon * pDungeon = q.GetCurrentDungeon();

		if (pDungeon)
			pDungeon->SpawnMoveGroup((uint32_t)lua_tonumber(L,1), lua_tostring(L,2), lua_tostring(L,3));

		return 0;
	}

	int dungeon_spawn_move_unique(lua_State* L)
	{
		if (!lua_isstring(L,1) || !lua_isnumber(L,2) || !lua_isstring(L,3) || !lua_isstring(L,4))
			return 0;
		metin2::log::info("QUEST_DUNGEON_SPAWN_MOVE_UNIQUE {} {:d} {} {}", lua_tostring(L,1), (int)lua_tonumber(L,2), lua_tostring(L,3), lua_tostring(L,4));

		CQuestManager& q = CQuestManager::instance();
		CDungeon * pDungeon = q.GetCurrentDungeon();

		if (pDungeon)
			pDungeon->SpawnMoveUnique(lua_tostring(L,1), (uint32_t)lua_tonumber(L,2), lua_tostring(L,3), lua_tostring(L,4));

		return 0;
	}

	int dungeon_spawn_unique(lua_State* L)
	{
		if (!lua_isstring(L,1) || !lua_isnumber(L,2) || !lua_isstring(L,3))
			return 0;
		metin2::log::info("QUEST_DUNGEON_SPAWN_UNIQUE {} {:d} {}", lua_tostring(L,1), (int)lua_tonumber(L,2), lua_tostring(L,3));

		CQuestManager& q = CQuestManager::instance();
		CDungeon * pDungeon = q.GetCurrentDungeon();

		if (pDungeon)
			pDungeon->SpawnUnique(lua_tostring(L,1), (uint32_t)lua_tonumber(L,2), lua_tostring(L,3));

		return 0;
	}

	int dungeon_spawn(lua_State* L)
	{
		if (!lua_isnumber(L,1) || !lua_isstring(L,2))
			return 0;
		metin2::log::info("QUEST_DUNGEON_SPAWN {:d} {}", (int)lua_tonumber(L,1), lua_tostring(L,2));

		CQuestManager& q = CQuestManager::instance();
		CDungeon * pDungeon = q.GetCurrentDungeon();

		if (pDungeon)
			pDungeon->Spawn((uint32_t)lua_tonumber(L,1), lua_tostring(L,2));

		return 0;
	}

	int dungeon_set_unique(lua_State* L)
	{
		if (!lua_isstring(L, 1) || !lua_isnumber(L, 2))
			return 0;

		CQuestManager& q = CQuestManager::instance();
		CDungeon * pDungeon = q.GetCurrentDungeon();

		uint32_t vid = (uint32_t) lua_tonumber(L, 2);

		if (pDungeon)
			pDungeon->SetUnique(lua_tostring(L, 1), vid);
		return 0;
	}

	int dungeon_get_unique_vid(lua_State* L)
	{
		if (!lua_isstring(L,1))
		{
			lua_pushnumber(L,0);
			return 1;
		}

		CQuestManager& q = CQuestManager::instance();
		CDungeon * pDungeon = q.GetCurrentDungeon();

		if (pDungeon)
		{
			lua_pushnumber(L, pDungeon->GetUniqueVid(lua_tostring(L,1)));
			return 1;
		}

		lua_pushnumber(L,0);
		return 1;
	}

	int dungeon_spawn_mob(lua_State* L)
	{
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3))
		{
			quest::error("invalid argument");
			return 0;
		}

		CQuestManager& q = CQuestManager::instance();
		CDungeon * pDungeon = q.GetCurrentDungeon();

		uint32_t vid = 0;

		if (pDungeon)
		{
			uint32_t dwVnum = (uint32_t) lua_tonumber(L, 1);
			int32_t x = (int32_t) lua_tonumber(L, 2);
			int32_t y = (int32_t) lua_tonumber(L, 3);
			float radius = lua_isnumber(L, 4) ? (float) lua_tonumber(L, 4) : 0;
			uint32_t count = (lua_isnumber(L, 5)) ? (uint32_t) lua_tonumber(L, 5) : 1;

			metin2::log::info( "dungeon_spawn_mob {:d} {:d} {:d}", dwVnum, x, y);

			if (count == 0)
				count = 1;

			while (count --)
			{
				if (radius<1)
				{
					CHARACTER * ch = pDungeon->SpawnMob(dwVnum, x, y);
					if (ch && !vid)
						vid = ch->GetVID();
				}
				else
				{
					float angle = number(0, 999) * std::numbers::pi * 2 / 1000;
					float r = number(0, 999) * radius / 1000;

					int32_t nx = x + (int32_t)(r * cos(angle));
					int32_t ny = y + (int32_t)(r * sin(angle));

					CHARACTER * ch = pDungeon->SpawnMob(dwVnum, nx, ny);
					if (ch && !vid)
						vid = ch->GetVID();
				}
			}
		}

		lua_pushnumber(L, vid);
		return 1;
	}

	int dungeon_spawn_mob_dir(lua_State* L)
	{
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3) || !lua_isnumber(L, 4))
		{
			quest::error("invalid argument");
			return 0;
		}

		CQuestManager& q = CQuestManager::instance();
		CDungeon * pDungeon = q.GetCurrentDungeon();

		uint32_t vid = 0;

		if (pDungeon)
		{
			uint32_t dwVnum = (uint32_t) lua_tonumber(L, 1);
			int32_t x = (int32_t) lua_tonumber(L, 2);
			int32_t y = (int32_t) lua_tonumber(L, 3);
			uint8_t dir = (int) lua_tonumber(L, 4);
			
			CHARACTER * ch = pDungeon->SpawnMob(dwVnum, x, y, dir);
			if (ch && !vid)
				vid = ch->GetVID();
		}
		lua_pushnumber(L, vid);
		return 1;
	}
	
	int dungeon_spawn_mob_ac_dir(lua_State* L)
	{
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3) || !lua_isnumber(L, 4))
		{
			quest::error("invalid argument");
			return 0;
		}

		CQuestManager& q = CQuestManager::instance();
		CDungeon * pDungeon = q.GetCurrentDungeon();

		uint32_t vid = 0;

		if (pDungeon)
		{
			uint32_t dwVnum = (uint32_t) lua_tonumber(L, 1);
			int32_t x = (int32_t) lua_tonumber(L, 2);
			int32_t y = (int32_t) lua_tonumber(L, 3);
			uint8_t dir = (int) lua_tonumber(L, 4);
			
			CHARACTER * ch = pDungeon->SpawnMob_ac_dir(dwVnum, x, y, dir);
			if (ch && !vid)
				vid = ch->GetVID();
		}
		lua_pushnumber(L, vid);
		return 1;
	}

	int dungeon_spawn_goto_mob(lua_State* L)
	{
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3) || !lua_isnumber(L, 4))
			return 0;

		int32_t lFromX = (int32_t)lua_tonumber(L, 1);
		int32_t lFromY = (int32_t)lua_tonumber(L, 2);
		int32_t lToX   = (int32_t)lua_tonumber(L, 3);
		int32_t lToY   = (int32_t)lua_tonumber(L, 4);

		CQuestManager& q = CQuestManager::instance();
		CDungeon * pDungeon = q.GetCurrentDungeon();

		if (pDungeon)
			pDungeon->SpawnGotoMob(lFromX, lFromY, lToX, lToY);

		return 0;
	}

	int dungeon_spawn_name_mob(lua_State* L)
	{
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3) || !lua_isstring(L, 4))
			return 0;

		CQuestManager& q = CQuestManager::instance();
		CDungeon * pDungeon = q.GetCurrentDungeon();

		if (pDungeon)
		{
			uint32_t dwVnum = (uint32_t) lua_tonumber(L, 1);
			int32_t x = (int32_t)lua_tonumber(L, 2);
			int32_t y = (int32_t)lua_tonumber(L, 3);
			pDungeon->SpawnNameMob(dwVnum, x, y, lua_tostring(L, 4));
		}
		return 0;
	}

	int dungeon_spawn_group(lua_State* L)
	{
		//
		// argument: vnum,x,y,radius,aggressive,count
		//
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3) || !lua_isnumber(L, 4) || !lua_isnumber(L, 6))
		{
			quest::error("invalid argument");
			return 0;
		}

		uint32_t vid = 0;

		CQuestManager& q = CQuestManager::instance();
		CDungeon * pDungeon = q.GetCurrentDungeon();

		if (pDungeon)
		{
			uint32_t group_vnum = (uint32_t)lua_tonumber(L, 1);
			int32_t local_x = (int32_t) lua_tonumber(L, 2) * 100;
			int32_t local_y = (int32_t) lua_tonumber(L, 3) * 100;
			float radius = (float) lua_tonumber(L, 4) * 100;
			bool bAggressive = lua_toboolean(L, 5);
			uint32_t count = (uint32_t) lua_tonumber(L, 6);

			CHARACTER * chRet = pDungeon->SpawnGroup(group_vnum, local_x, local_y, radius, bAggressive, count);
			if (chRet)
				vid = chRet->GetVID();
		}

		lua_pushnumber(L, vid);
		return 1;
	}

	int dungeon_join(lua_State* L)
	{
		if (lua_gettop(L) < 1 || !lua_isnumber(L, 1))
			return 0;

		int32_t lMapIndex = (int32_t)lua_tonumber(L, 1);
		CDungeon * pDungeon = CDungeonManager::instance().Create(lMapIndex);

		if (!pDungeon)
			return 0;

		CHARACTER * ch = CQuestManager::instance().GetCurrentCharacterPtr();

		if (ch->GetParty() && ch->GetParty()->GetLeaderPID() == ch->GetPlayerID())
			pDungeon->JoinParty(ch->GetParty());
		else if (!ch->GetParty())
			pDungeon->Join(ch);

		return 0;
	}

	int dungeon_exit(lua_State* L) // send to the position before entering the dungeon
	{
		CHARACTER * ch = CQuestManager::instance().GetCurrentCharacterPtr();

		ch->ExitToSavedLocation();
		return 0;
	}

	int dungeon_exit_all(lua_State* L) // send everyone in the dungeon to their position before entering the dungeon
	{
		CQuestManager& q = CQuestManager::instance();
		CDungeon * pDungeon = q.GetCurrentDungeon();

		if (pDungeon)
			pDungeon->ExitAll();

		return 0;
	}

	struct FSayDungeonByItemGroup
	{
		const CDungeon::ItemGroup* item_group;
		std::string can_enter_ment;
		std::string cant_enter_ment;
		void operator()(CEntity * ent)
		{
			if (ent->IsType(ENTITY_CHARACTER))
			{
				CHARACTER * ch = (CHARACTER *) ent;

				if (ch->IsPC())
				{
					struct metin2::game::packet_script packet_script;
					TEMP_BUFFER buf;
					
					for (CDungeon::ItemGroup::const_iterator it = item_group->begin(); it != item_group->end(); it++)
					{
						if(ch->CountSpecifyItem(it->first) >= it->second)
						{
							packet_script.header = HEADER_GC_SCRIPT;
							packet_script.skin = quest::CQuestManager::QUEST_SKIN_NORMAL;
							packet_script.src_size = can_enter_ment.size();
							packet_script.size = packet_script.src_size + sizeof(struct packet_script);

							buf.write(&packet_script, sizeof(struct packet_script));
							buf.write(&can_enter_ment[0], can_enter_ment.size());
							ch->GetDesc()->Packet(buf.read_peek(), buf.size());
							return;
						}
					}

					packet_script.header = HEADER_GC_SCRIPT;
					packet_script.skin = quest::CQuestManager::QUEST_SKIN_NORMAL;
					packet_script.src_size = cant_enter_ment.size();
					packet_script.size = packet_script.src_size + sizeof(struct packet_script);

					buf.write(&packet_script, sizeof(struct packet_script));
					buf.write(&cant_enter_ment[0], cant_enter_ment.size());
					ch->GetDesc()->Packet(buf.read_peek(), buf.size());
				}
			}
		}
	};


	int dungeon_say_diff_by_item_group(lua_State* L)
	{
		if (!lua_isstring(L, 1) || !lua_isstring(L, 2) || !lua_isstring(L, 3))
		{
			metin2::log::info( "QUEST wrong set flag");
			return 0;
		}

		CQuestManager& q = CQuestManager::instance();
		CDungeon * pDungeon = q.GetCurrentDungeon();

		if(!pDungeon)
		{
			quest::error("QUEST : no dungeon");
			return 0;
		}

		SECTREE_MAP * pMap = SECTREE_MANAGER::instance().GetMap(pDungeon->GetMapIndex());

		if (!pMap)
		{
			quest::error("cannot find map by index {:d}", pDungeon->GetMapIndex());
			return 0;
		}
		FSayDungeonByItemGroup f;
		metin2::log::info("diff_by_item");
	
		std::string group_name (lua_tostring (L, 1));
		f.item_group = pDungeon->GetItemGroup (group_name);

		if (f.item_group == nullptr)
		{
			quest::error("invalid item group");
			return 0;
		}

		f.can_enter_ment = lua_tostring(L, 2);
		f.can_enter_ment+= "[ENTER][ENTER][ENTER][ENTER][DONE]";
		f.cant_enter_ment = lua_tostring(L, 3);
		f.cant_enter_ment+= "[ENTER][ENTER][ENTER][ENTER][DONE]";

		pMap -> for_each( f );

		return 0;
	}
	
	struct FExitDungeonByItemGroup
	{
		const CDungeon::ItemGroup* item_group;
		
		void operator()(CEntity * ent)
		{
			if (ent->IsType(ENTITY_CHARACTER))
			{
				CHARACTER * ch = (CHARACTER *) ent;

				if (ch->IsPC())
				{
					for (CDungeon::ItemGroup::const_iterator it = item_group->begin(); it != item_group->end(); it++)
					{
						if(ch->CountSpecifyItem(it->first) >= it->second)
						{
							return;
						}
					}
					ch->ExitToSavedLocation();
				}
			}
		}
	};
	
	int dungeon_exit_all_by_item_group (lua_State* L) // kick anyone who doesn't have an item from a specific item group
	{
		if (!lua_isstring(L, 1))
		{
			metin2::log::info( "QUEST wrong set flag");
			return 0;
		}

		CQuestManager& q = CQuestManager::instance();
		CDungeon * pDungeon = q.GetCurrentDungeon();

		if(!pDungeon)
		{
			quest::error("QUEST : no dungeon");
			return 0;
		}

		SECTREE_MAP * pMap = SECTREE_MANAGER::instance().GetMap(pDungeon->GetMapIndex());

		if (!pMap)
		{
			quest::error("cannot find map by index {:d}", pDungeon->GetMapIndex());
			return 0;
		}
		FExitDungeonByItemGroup f;

		std::string group_name (lua_tostring (L, 1));
		f.item_group = pDungeon->GetItemGroup (group_name);
	
		if (f.item_group == nullptr)
		{
			quest::error("invalid item group");
			return 0;
		}
		
		pMap -> for_each( f );
		
		return 0;
	}

	struct FDeleteItemInItemGroup
	{
		const CDungeon::ItemGroup* item_group;

		void operator()(CEntity * ent)
		{
			if (ent->IsType(ENTITY_CHARACTER))
			{
				CHARACTER * ch = (CHARACTER *) ent;

				if (ch->IsPC())
				{
					for (CDungeon::ItemGroup::const_iterator it = item_group->begin(); it != item_group->end(); it++)
					{
						if(ch->CountSpecifyItem(it->first) >= it->second)
						{
							ch->RemoveSpecifyItem (it->first, it->second);
							return;
						}
					}
				}
			}
		}
	};
	
	int dungeon_delete_item_in_item_group_from_all(lua_State* L) // remove a specific item from PCs in the dungeon.
	{
		if (!lua_isstring(L, 1))
		{
			metin2::log::info( "QUEST wrong set flag");
			return 0;
		}

		CQuestManager& q = CQuestManager::instance();
		CDungeon * pDungeon = q.GetCurrentDungeon();

		if(!pDungeon)
		{
			quest::error("QUEST : no dungeon");
			return 0;
		}

		SECTREE_MAP * pMap = SECTREE_MANAGER::instance().GetMap(pDungeon->GetMapIndex());

		if (!pMap)
		{
			quest::error("cannot find map by index {:d}", pDungeon->GetMapIndex());
			return 0;
		}
		FDeleteItemInItemGroup f;

		std::string group_name (lua_tostring (L, 1));
		f.item_group = pDungeon->GetItemGroup (group_name);

		if (f.item_group == nullptr)
		{
			quest::error("invalid item group");
			return 0;
		}

		pMap -> for_each( f );
		
		return 0;
	}


	int dungeon_kill_all(lua_State* L)
	{
		CQuestManager& q = CQuestManager::instance();
		CDungeon * pDungeon = q.GetCurrentDungeon();

		if (pDungeon)
			pDungeon->KillAll();

		return 0;
	}

	int dungeon_purge(lua_State* L)
	{
		CQuestManager& q = CQuestManager::instance();
		CDungeon * pDungeon = q.GetCurrentDungeon();

		if (pDungeon)
			pDungeon->Purge();

		return 0;
	}

	int dungeon_exit_all_to_start_position(lua_State * L)
	{
		CQuestManager& q = CQuestManager::instance();
		CDungeon * pDungeon = q.GetCurrentDungeon();

		if (pDungeon)
			pDungeon->ExitAllToStartPosition();

		return 0;
	}

	int dungeon_count_monster(lua_State * L)
	{
		CQuestManager& q = CQuestManager::instance();
		CDungeon * pDungeon = q.GetCurrentDungeon();

		if (pDungeon)
			lua_pushnumber(L, pDungeon->CountMonster());
		else
		{
			quest::error("not in a dungeon");
			lua_pushnumber(L, INT32_MAX);
		}

		return 1;
	}

	int dungeon_select(lua_State* L)
	{
		uint32_t dwMapIndex = (uint32_t) lua_tonumber(L, 1);
		if (dwMapIndex)
		{
			CDungeon * pDungeon = CDungeonManager::instance().FindByMapIndex(dwMapIndex);
			if (pDungeon)
			{
				CQuestManager::instance().SelectDungeon(pDungeon);
				lua_pushboolean(L, 1);
			}
			else
			{
				CQuestManager::instance().SelectDungeon(nullptr);
				lua_pushboolean(L, 0);
			}
		}
		else
		{
			CQuestManager::instance().SelectDungeon(nullptr);
			lua_pushboolean(L, 0);
		}
		return 1;
	}

	int dungeon_find(lua_State* L)
	{
		uint32_t dwMapIndex = (uint32_t) lua_tonumber(L, 1);
		if (dwMapIndex)
		{
			CDungeon * pDungeon = CDungeonManager::instance().FindByMapIndex(dwMapIndex);
			if (pDungeon)
			{
				lua_pushboolean(L, 1);
			}
			else
			{
				lua_pushboolean(L, 0);
			}
		}
		else
		{
			lua_pushboolean(L, 0);
		}
		return 1;
	}

	int dungeon_all_near_to( lua_State* L)
	{
		CDungeon * pDungeon = CQuestManager::instance().GetCurrentDungeon();

		if (pDungeon != nullptr)
		{
			lua_pushboolean(L, pDungeon->IsAllPCNearTo( (int)lua_tonumber(L, 1), (int)lua_tonumber(L, 2), 30));
		}
		else
		{
			lua_pushboolean(L, false);
		}

		return 1;
	}

	int dungeon_set_warp_location (lua_State* L)
	{
		CDungeon * pDungeon = CQuestManager::instance().GetCurrentDungeon();

		if (pDungeon == nullptr)
		{
			return 0;
		}
		
		if (lua_gettop(L)<3 || !lua_isnumber(L, 1) || !lua_isnumber(L,2) || !lua_isnumber(L, 3))
		{
			return 0;
		}

		FSetWarpLocation f ((int)lua_tonumber(L, 1), (int)lua_tonumber(L, 2), (int)lua_tonumber(L, 3));
		pDungeon->ForEachMember (f);

		return 0;
	}

	int dungeon_set_item_group (lua_State* L)
	{
		if (!lua_isstring (L, 1) || !lua_isnumber (L, 2))
		{
			return 0;
		}
		std::string group_name (lua_tostring (L, 1));
		int size = lua_tonumber (L, 2);

		CDungeon::ItemGroup item_group;
		
		for (int i = 0; i < size; i++)
		{
			if (!lua_isnumber (L, i * 2 + 3) || !lua_isnumber (L, i * 2 + 4))
			{
				return 0;
			}
			item_group.push_back (std::pair <uint32_t, int> (lua_tonumber (L, i * 2 + 3), lua_tonumber (L, i * 2 + 4)));
		}
		CDungeon * pDungeon = CQuestManager::instance().GetCurrentDungeon();

		if (pDungeon == nullptr)
		{
			return 0;
		}
		
		pDungeon->CreateItemGroup (group_name, item_group);
		return 0;
	}

	int dungeon_set_quest_flag2(lua_State* L)
	{
		CQuestManager & q = CQuestManager::instance();

		FSetQuestFlag f;

		if (!lua_isstring(L, 1) || !lua_isstring(L, 2) || !lua_isnumber(L, 3))
		{
			quest::error("Invalid Argument");
		}

		f.flagname = std::string (lua_tostring(L, 1)) + "." + lua_tostring(L, 2);
		f.value = (int) rint(lua_tonumber(L, 3));

		CDungeon * pDungeon = q.GetCurrentDungeon();

		if (pDungeon)
			pDungeon->ForEachMember(f);

		return 0;
	}

	void RegisterDungeonFunctionTable() 
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

		CQuestManager::instance().AddLuaFunctionTable("d", dungeon_functions);
	}
}
}
