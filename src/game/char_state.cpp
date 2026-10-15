#include "affect.h"
#include "BlueDragon.h"
#include "char.h"
#include "char_manager.h"
#include "config.h"
#include "constants.h"
#include "entity.h"
#include "exchange.h"
#include "guild_manager.h"
#include "locale.hpp"
#include "mob_manager.h"
#include "motion.h"
#include "p2p.h"
#include "packet.h"
#include "party.h"
#include "questmanager.h"
#include "sectree_manager.h"
#include "typedef.h"
#include "utils.h"
#include "vector.h"
#include "war_map.h"
#include "xmas_event.h"

#include <common/length.h>
#include <metin2/log/log.h>
#include <libthecore/main.h>
#include <libthecore/utils.h>

#include <algorithm>
#include <climits>
#include <cstdint>
#include <cstdio>


namespace
{
	class FuncFindChrForFlag
	{
		public:
			FuncFindChrForFlag(CHARACTER * pkChr) :
				m_pkChr(pkChr), m_pkChrFind(nullptr), m_iMinDistance(INT_MAX)
				{
				}

			void operator () (CEntity * ent)
			{
				if (!ent->IsType(ENTITY_CHARACTER))
					return;

				if (ent->IsObserverMode())
					return;

				CHARACTER * pkChr = (CHARACTER *) ent;

				if (!pkChr->IsPC())
					return;

				if (!pkChr->GetGuild())
					return;

				if (pkChr->IsDead())
					return;

				int iDist = DISTANCE_APPROX(pkChr->GetX()-m_pkChr->GetX(), pkChr->GetY()-m_pkChr->GetY());

				if (iDist <= 500 && m_iMinDistance > iDist &&
						!pkChr->IsAffectFlag(AFF_WAR_FLAG1) &&
						!pkChr->IsAffectFlag(AFF_WAR_FLAG2) &&
						!pkChr->IsAffectFlag(AFF_WAR_FLAG3))
				{
					// If it's our team's flag
					if ((uint32_t) m_pkChr->GetPoint(POINT_STAT) == pkChr->GetGuild()->GetID())
					{
						CWarMap * pMap = pkChr->GetWarMap();
						uint8_t idx;

						if (!pMap || !pMap->GetTeamIndex(pkChr->GetGuild()->GetID(), idx))
							return;

						// Only pick up the flag when there is no flag at our base. Otherwise the flag at the base
						// could get picked up even when we want to leave it alone..
						if (!pMap->IsFlagOnBase(idx))
						{
							m_pkChrFind = pkChr;
							m_iMinDistance = iDist;
						}
					}
					else
					{
						// Always pick up the enemy team's flag.
						m_pkChrFind = pkChr;
						m_iMinDistance = iDist;
					}
				}
			}

			CHARACTER *	m_pkChr;
			CHARACTER * m_pkChrFind;
			int		m_iMinDistance;
	};

	class FuncFindChrForFlagBase
	{
		public:
			FuncFindChrForFlagBase(CHARACTER * pkChr) : m_pkChr(pkChr)
			{
			}

			void operator () (CEntity * ent)
			{
				if (!ent->IsType(ENTITY_CHARACTER))
					return;

				if (ent->IsObserverMode())
					return;

				CHARACTER * pkChr = (CHARACTER *) ent;

				if (!pkChr->IsPC())
					return;

				CGuild * pkGuild = pkChr->GetGuild();

				if (!pkGuild)
					return;

				int iDist = DISTANCE_APPROX(pkChr->GetX()-m_pkChr->GetX(), pkChr->GetY()-m_pkChr->GetY());

				if (iDist <= 500 &&
						(pkChr->IsAffectFlag(AFF_WAR_FLAG1) || 
						 pkChr->IsAffectFlag(AFF_WAR_FLAG2) ||
						 pkChr->IsAffectFlag(AFF_WAR_FLAG3)))
				{
					CAffect * pkAff = pkChr->FindAffect(AFFECT_WAR_FLAG);

					metin2::log::info( "FlagBase {} dist {:d} aff {} flag gid {:d} chr gid {:d}",
							pkChr->GetName(), iDist, static_cast<const void *>(pkAff), m_pkChr->GetPoint(POINT_STAT),
							pkChr->GetGuild()->GetID());

					if (pkAff)
					{
						if ((uint32_t) m_pkChr->GetPoint(POINT_STAT) == pkGuild->GetID() &&
								m_pkChr->GetPoint(POINT_STAT) != pkAff->lApplyValue)
						{
							CWarMap * pMap = pkChr->GetWarMap();
							uint8_t idx;

							if (!pMap || !pMap->GetTeamIndex(pkGuild->GetID(), idx))
								return;

							//if (pMap->IsFlagOnBase(idx))
							{
								uint8_t idx_opp = idx == 0 ? 1 : 0;

								SendGuildWarScore(m_pkChr->GetPoint(POINT_STAT), pkAff->lApplyValue, 1);

								pMap->ResetFlag();

								char buf[256];
								snprintf(buf, sizeof(buf), LC_TEXT("%s 길드가 %s 길드의 깃발을 빼앗았습니다!"), pMap->GetGuild(idx)->GetName(), pMap->GetGuild(idx_opp)->GetName());
								pMap->Notice(buf);
							}
						}
					}
				}
			}

			CHARACTER * m_pkChr;
	};

	class FuncFindGuardVictim
	{
		public:
			FuncFindGuardVictim(CHARACTER * pkChr, int iMaxDistance) :
			m_iMinDistance(INT_MAX),
			m_iMaxDistance(iMaxDistance),
			m_lx(pkChr->GetX()),
			m_ly(pkChr->GetY()),
			m_pkChrVictim(nullptr)
			{
			};

			void operator () (CEntity * ent)
			{
				if (!ent->IsType(ENTITY_CHARACTER))
					return;

				CHARACTER * pkChr = (CHARACTER *) ent;

				// Don't attack PCs for now
				if (pkChr->IsPC())
					return;

				if (pkChr->IsNPC() && !pkChr->IsMonster())
					return;

				if (pkChr->IsDead())
					return;

				if (pkChr->IsAffectFlag(AFF_EUNHYUNG) || 
						pkChr->IsAffectFlag(AFF_INVISIBILITY) ||
						pkChr->IsAffectFlag(AFF_REVIVE_INVISIBLE))
					return;

				// Skip Japanese pirates (Wae-gu)
				if (pkChr->GetRaceNum() == 5001)
					return;

				int iDistance = DISTANCE_APPROX(m_lx - pkChr->GetX(), m_ly - pkChr->GetY());

				if (iDistance < m_iMinDistance && iDistance <= m_iMaxDistance)
				{
					m_pkChrVictim = pkChr;
					m_iMinDistance = iDistance;
				}
			}

			CHARACTER * GetVictim()
			{
				return (m_pkChrVictim);
			}

		private:
			int		m_iMinDistance;
			int		m_iMaxDistance;
			int32_t	m_lx;
			int32_t	m_ly;

			CHARACTER *	m_pkChrVictim;
	};

}

bool CHARACTER::IsAggressive() const
{
	return IS_SET(m_pointsInstant.dwAIFlag, AIFLAG_AGGRESSIVE);
}

void CHARACTER::SetAggressive()
{
	SET_BIT(m_pointsInstant.dwAIFlag, AIFLAG_AGGRESSIVE);
}

bool CHARACTER::IsCoward() const
{
	return IS_SET(m_pointsInstant.dwAIFlag, AIFLAG_COWARD);
}

void CHARACTER::SetCoward()
{
	SET_BIT(m_pointsInstant.dwAIFlag, AIFLAG_COWARD);
}

bool CHARACTER::IsBerserker() const
{
	return IS_SET(m_pointsInstant.dwAIFlag, AIFLAG_BERSERK);
}

bool CHARACTER::IsStoneSkinner() const
{
	return IS_SET(m_pointsInstant.dwAIFlag, AIFLAG_STONESKIN);
}

bool CHARACTER::IsGodSpeeder() const
{
	return IS_SET(m_pointsInstant.dwAIFlag, AIFLAG_GODSPEED);
}

bool CHARACTER::IsDeathBlower() const
{
	return IS_SET(m_pointsInstant.dwAIFlag, AIFLAG_DEATHBLOW);
}

bool CHARACTER::IsReviver() const
{
	return IS_SET(m_pointsInstant.dwAIFlag, AIFLAG_REVIVE);
}

void CHARACTER::CowardEscape()
{
	int iDist[4] = {500, 1000, 3000, 5000};

	for (int iDistIdx = 2; iDistIdx >= 0; --iDistIdx)
		for (int iTryCount = 0; iTryCount < 8; ++iTryCount)
		{
			SetRotation(number(0, 359));        // Pick a random direction

			float fx, fy;
			float fDist = number(iDist[iDistIdx], iDist[iDistIdx+1]);

			GetDeltaByDegree(GetRotation(), fDist, &fx, &fy);

			bool bIsWayBlocked = false;
			for (int j = 1; j <= 100; ++j)
			{
				if (!SECTREE_MANAGER::instance().IsMovablePosition(GetMapIndex(), GetX() + (int) fx*j/100, GetY() + (int) fy*j/100))
				{
					bIsWayBlocked = true;
					break;
				}
			}

			if (bIsWayBlocked)
				continue;

			m_dwStateDuration = PASSES_PER_SEC(1);

			int iDestX = GetX() + (int) fx;
			int iDestY = GetY() + (int) fy;

			if (Goto(iDestX, iDestY))
				SendMovePacket(FUNC_WAIT, 0, 0, 0, 0);

			metin2::log::info( "WAEGU move to {:d} {:d} (far)", iDestX, iDestY);
			return;
		}
}

bool CHARACTER::IsNoAttackShinsu() const
{
	return IS_SET(m_pointsInstant.dwAIFlag, AIFLAG_NOATTACKSHINSU);
}

bool CHARACTER::IsNoAttackChunjo() const
{
	return IS_SET(m_pointsInstant.dwAIFlag, AIFLAG_NOATTACKCHUNJO);
}

bool CHARACTER::IsNoAttackJinno() const
{
	return IS_SET(m_pointsInstant.dwAIFlag, AIFLAG_NOATTACKJINNO);
}

bool CHARACTER::IsAttackMob() const
{
	return IS_SET(m_pointsInstant.dwAIFlag, AIFLAG_ATTACKMOB);
}

// STATE_IDLE_REFACTORING
void CHARACTER::StateIdle()
{
	if (IsStone())
	{
		__StateIdle_Stone();
		return;
	}
	else if (IsWarp() || IsGoto())
	{
		// Warp is handled by an event
		m_dwStateDuration = 60 * metin2::config::server().passes_per_sec;
		return;
	}

	if (IsPC())
		return;

	// NPC handling
	if (!IsMonster())
	{
		__StateIdle_NPC();
		return;
	}

	__StateIdle_Monster();
}

void CHARACTER::__StateIdle_Stone()
{
	m_dwStateDuration = PASSES_PER_SEC(1);

	int iPercent = (GetHP() * 100) / GetMaxHP();
	uint32_t dwVnum = number(std::min<int>(GetMobTable().sAttackSpeed, GetMobTable().sMovingSpeed ), std::max<int>(GetMobTable().sAttackSpeed, GetMobTable().sMovingSpeed));

	if (iPercent <= 10 && GetMaxSP() < 10)
	{
		SetMaxSP(10);
		SendMovePacket(FUNC_ATTACK, 0, GetX(), GetY(), 0);

		CHARACTER_MANAGER::instance().SelectStone(this);
		CHARACTER_MANAGER::instance().SpawnGroup(dwVnum, GetMapIndex(), GetX() - 500, GetY() - 500, GetX() + 500, GetY() + 500);
		CHARACTER_MANAGER::instance().SpawnGroup(dwVnum, GetMapIndex(), GetX() - 1000, GetY() - 1000, GetX() + 1000, GetY() + 1000);
		CHARACTER_MANAGER::instance().SpawnGroup(dwVnum, GetMapIndex(), GetX() - 1500, GetY() - 1500, GetX() + 1500, GetY() + 1500);
		CHARACTER_MANAGER::instance().SelectStone(nullptr);
	}
	else if (iPercent <= 20 && GetMaxSP() < 9)
	{
		SetMaxSP(9);
		SendMovePacket(FUNC_ATTACK, 0, GetX(), GetY(), 0);

		CHARACTER_MANAGER::instance().SelectStone(this);
		CHARACTER_MANAGER::instance().SpawnGroup(dwVnum, GetMapIndex(), GetX() - 500, GetY() - 500, GetX() + 500, GetY() + 500);
		CHARACTER_MANAGER::instance().SpawnGroup(dwVnum, GetMapIndex(), GetX() - 1000, GetY() - 1000, GetX() + 1000, GetY() + 1000);
		CHARACTER_MANAGER::instance().SpawnGroup(dwVnum, GetMapIndex(), GetX() - 1500, GetY() - 1500, GetX() + 1500, GetY() + 1500);
		CHARACTER_MANAGER::instance().SelectStone(nullptr);
	}
	else if (iPercent <= 30 && GetMaxSP() < 8)
	{
		SetMaxSP(8);
		SendMovePacket(FUNC_ATTACK, 0, GetX(), GetY(), 0);

		CHARACTER_MANAGER::instance().SelectStone(this);
		CHARACTER_MANAGER::instance().SpawnGroup(dwVnum, GetMapIndex(), GetX() - 500, GetY() - 500, GetX() + 500, GetY() + 500);
		CHARACTER_MANAGER::instance().SpawnGroup(dwVnum, GetMapIndex(), GetX() - 1000, GetY() - 1000, GetX() + 1000, GetY() + 1000);
		CHARACTER_MANAGER::instance().SpawnGroup(dwVnum, GetMapIndex(), GetX() - 1000, GetY() - 1000, GetX() + 1000, GetY() + 1000);
		CHARACTER_MANAGER::instance().SelectStone(nullptr);
	}
	else if (iPercent <= 40 && GetMaxSP() < 7)
	{
		SetMaxSP(7);
		SendMovePacket(FUNC_ATTACK, 0, GetX(), GetY(), 0);

		CHARACTER_MANAGER::instance().SelectStone(this);
		CHARACTER_MANAGER::instance().SpawnGroup(dwVnum, GetMapIndex(), GetX() - 1000, GetY() - 1000, GetX() + 1000, GetY() + 1000);
		CHARACTER_MANAGER::instance().SpawnGroup(dwVnum, GetMapIndex(), GetX() - 1000, GetY() - 1000, GetX() + 1000, GetY() + 1000);
		CHARACTER_MANAGER::instance().SpawnGroup(dwVnum, GetMapIndex(), GetX() - 1000, GetY() - 1000, GetX() + 1000, GetY() + 1000);
		CHARACTER_MANAGER::instance().SelectStone(nullptr);
	}
	else if (iPercent <= 50 && GetMaxSP() < 6)
	{
		SetMaxSP(6);
		SendMovePacket(FUNC_ATTACK, 0, GetX(), GetY(), 0);

		CHARACTER_MANAGER::instance().SelectStone(this);
		CHARACTER_MANAGER::instance().SpawnGroup(dwVnum, GetMapIndex(), GetX() - 1000, GetY() - 1000, GetX() + 1000, GetY() + 1000);
		CHARACTER_MANAGER::instance().SpawnGroup(dwVnum, GetMapIndex(), GetX() - 1000, GetY() - 1000, GetX() + 1000, GetY() + 1000);
		CHARACTER_MANAGER::instance().SelectStone(nullptr);
	}
	else if (iPercent <= 60 && GetMaxSP() < 5)
	{
		SetMaxSP(5);
		SendMovePacket(FUNC_ATTACK, 0, GetX(), GetY(), 0);

		CHARACTER_MANAGER::instance().SelectStone(this);
		CHARACTER_MANAGER::instance().SpawnGroup(dwVnum, GetMapIndex(), GetX() - 1000, GetY() - 1000, GetX() + 1000, GetY() + 1000);
		CHARACTER_MANAGER::instance().SpawnGroup(dwVnum, GetMapIndex(), GetX() - 500, GetY() - 500, GetX() + 500, GetY() + 500);
		CHARACTER_MANAGER::instance().SelectStone(nullptr);
	}
	else if (iPercent <= 70 && GetMaxSP() < 4)
	{
		SetMaxSP(4);
		SendMovePacket(FUNC_ATTACK, 0, GetX(), GetY(), 0);

		CHARACTER_MANAGER::instance().SelectStone(this);
		CHARACTER_MANAGER::instance().SpawnGroup(dwVnum, GetMapIndex(), GetX() - 500, GetY() - 500, GetX() + 500, GetY() + 500);
		CHARACTER_MANAGER::instance().SpawnGroup(dwVnum, GetMapIndex(), GetX() - 1000, GetY() - 1000, GetX() + 1000, GetY() + 1000);
		CHARACTER_MANAGER::instance().SelectStone(nullptr);
	}
	else if (iPercent <= 80 && GetMaxSP() < 3)
	{
		SetMaxSP(3);
		SendMovePacket(FUNC_ATTACK, 0, GetX(), GetY(), 0);

		CHARACTER_MANAGER::instance().SelectStone(this);
		CHARACTER_MANAGER::instance().SpawnGroup(dwVnum, GetMapIndex(), GetX() - 1000, GetY() - 1000, GetX() + 1000, GetY() + 1000);
		CHARACTER_MANAGER::instance().SpawnGroup(dwVnum, GetMapIndex(), GetX() - 1000, GetY() - 1000, GetX() + 1000, GetY() + 1000);
		CHARACTER_MANAGER::instance().SelectStone(nullptr);
	}
	else if (iPercent <= 90 && GetMaxSP() < 2)
	{
		SetMaxSP(2);
		SendMovePacket(FUNC_ATTACK, 0, GetX(), GetY(), 0);

		CHARACTER_MANAGER::instance().SelectStone(this);
		CHARACTER_MANAGER::instance().SpawnGroup(dwVnum, GetMapIndex(), GetX() - 500, GetY() - 500, GetX() + 500, GetY() + 500);
		CHARACTER_MANAGER::instance().SelectStone(nullptr);
	}
	else if (iPercent <= 99 && GetMaxSP() < 1)
	{
		SetMaxSP(1);
		SendMovePacket(FUNC_ATTACK, 0, GetX(), GetY(), 0);

		CHARACTER_MANAGER::instance().SelectStone(this);
		CHARACTER_MANAGER::instance().SpawnGroup(dwVnum, GetMapIndex(), GetX() - 1000, GetY() - 1000, GetX() + 1000, GetY() + 1000);
		CHARACTER_MANAGER::instance().SelectStone(nullptr);
	}
	else
		return;

	UpdatePacket();
	return;
}

void CHARACTER::__StateIdle_NPC()
{
	MonsterChat(MONSTER_CHAT_WAIT);
	m_dwStateDuration = PASSES_PER_SEC(5);

	// The pet system's Idle is handled in CPetActor::Update, not in the state machine shared by almost all other character types.
	if (IsPet())
		return;
	else if (IsGuardNPC())
	{
		if (!quest::CQuestManager::instance().GetEventFlag("noguard"))
		{
			FuncFindGuardVictim f(this, 50000);

			if (GetSectree())
				GetSectree()->ForEachAround(f);

			CHARACTER * victim = f.GetVictim();

			if (victim)
			{
				m_dwStateDuration = metin2::config::server().passes_per_sec/2;

				if (CanBeginFight())
					BeginFight(victim);
			}
		}
	}
	else
	{
		if (GetRaceNum() == xmas::MOB_SANTA_VNUM) // Santa
		{
			if (get_dword_time() > m_dwPlayStartTime)
			{
				int	next_warp_time = 2 * 1000;	// 2 seconds

				m_dwPlayStartTime = get_dword_time() + next_warp_time;

				// Time's up, let's warp.
				// Sinseonja Noh Hae-yong
				const int WARP_MAP_INDEX_NUM = 7;
				static const int32_t c_lWarpMapIndexs[WARP_MAP_INDEX_NUM] = { 61, 62, 63, 64, 3, 23, 43 };
				int32_t lNextMapIndex;
				lNextMapIndex = c_lWarpMapIndexs[number(1, WARP_MAP_INDEX_NUM) - 1];

				if (map_allow_find(lNextMapIndex))
				{
					// It's here.
					CHARACTER_MANAGER::instance().DestroyCharacter(this);
					int iNextSpawnDelay = 0;
					iNextSpawnDelay = 50 * 60;

					xmas::SpawnSanta(lNextMapIndex, iNextSpawnDelay);
				}
				else
				{
					// It's another server.
					TPacketGGXmasWarpSanta p;
					p.bHeader   = HEADER_GG_XMAS_WARP_SANTA;
					p.bChannel  = metin2::config::server().channel;
					p.lMapIndex = lNextMapIndex;
					P2P_MANAGER::instance().Send(&p, sizeof(TPacketGGXmasWarpSanta));
				}
				return;
			}
		}

		if (!IS_SET(m_pointsInstant.dwAIFlag, AIFLAG_NOMOVE))
		{
			//
			// Move around here and there.
			// 
			CHARACTER * pkChrProtege = GetProtege();

			if (pkChrProtege)
			{
				if (DISTANCE_APPROX(GetX() - pkChrProtege->GetX(), GetY() - pkChrProtege->GetY()) > 500)
				{
					if (Follow(pkChrProtege, number(100, 300)))
						return;
				}
			}

			if (!number(0, 6))
			{
				SetRotation(number(0, 359));        // Pick a random direction

				float fx, fy;
				float fDist = number(200, 400);

				GetDeltaByDegree(GetRotation(), fDist, &fx, &fy);

				// Loose blocked-attribute check; don't go if the final or midpoint position is unreachable.
				if (!(SECTREE_MANAGER::instance().IsMovablePosition(GetMapIndex(), GetX() + (int) fx, GetY() + (int) fy) 
					&& SECTREE_MANAGER::instance().IsMovablePosition(GetMapIndex(), GetX() + (int) fx / 2, GetY() + (int) fy / 2)))
					return;

				SetNowWalking(true);

				if (Goto(GetX() + (int) fx, GetY() + (int) fy))
					SendMovePacket(FUNC_WAIT, 0, 0, 0, 0);

				return;
			}
		}
	}
}

void CHARACTER::__StateIdle_Monster()
{
	if (IsStun())
		return;

	if (!CanMove())
		return;

	if (IsCoward())
	{
		// Cowardly monsters only run away.
		if (!IsDead())
			CowardEscape();

		return;
	}

	if (IsBerserker())
		if (IsBerserk())
			SetBerserk(false);

	if (IsGodSpeeder())
		if (IsGodSpeed())
			SetGodSpeed(false);

	CHARACTER * victim = GetVictim();

	if (!victim || victim->IsDead())
	{
		SetVictim(nullptr);
		victim = nullptr;
		m_dwStateDuration = PASSES_PER_SEC(1);
	}

	if (!victim || victim->IsBuilding())
	{
		// Stone protection handling
		if (m_pkChrStone)
		{
			victim = m_pkChrStone->GetNearestVictim(m_pkChrStone);
		}
		// Aggressive monster handling
		else if (!metin2::config::game().no_wander && IsAggressive())
		{
			if (GetMapIndex() == 61 && quest::CQuestManager::instance().GetEventFlag("xmas_tree"));
			// On Seohan Mountain, don't attack first if there is a tree.
			else
				victim = FindVictim(this, m_pkMobData->m_table.wAggressiveSight);
		}
	}

	if (victim && !victim->IsDead())
	{
		if (CanBeginFight())
			BeginFight(victim);

		return;
	}

	if (IsAggressive() && !victim)
		m_dwStateDuration = PASSES_PER_SEC(number(1, 3));
	else
		m_dwStateDuration = PASSES_PER_SEC(number(3, 5));

	CHARACTER * pkChrProtege = GetProtege();

	// Follow if far from what we protect (stone, party leader).
	if (pkChrProtege)
	{
		if (DISTANCE_APPROX(GetX() - pkChrProtege->GetX(), GetY() - pkChrProtege->GetY()) > 1000)
		{
			if (Follow(pkChrProtege, number(150, 400)))
			{
				MonsterLog("[IDLE] 리더로부터 너무 멀리 떨어졌다! 복귀한다.");
				return;
			}
		}
	}

	//
	// Just wander back and forth.
	//
	if (!metin2::config::game().no_wander && !IS_SET(m_pointsInstant.dwAIFlag, AIFLAG_NOMOVE))
	{
		if (!number(0, 6))
		{
			SetRotation(number(0, 359));        // Pick a random direction

			float fx, fy;
			float fDist = number(300, 700);

			GetDeltaByDegree(GetRotation(), fDist, &fx, &fy);

			// Loose blocked-attribute check; don't go if the final or midpoint position is unreachable.
			if (!(SECTREE_MANAGER::instance().IsMovablePosition(GetMapIndex(), GetX() + (int) fx, GetY() + (int) fy) 
						&& SECTREE_MANAGER::instance().IsMovablePosition(GetMapIndex(), GetX() + (int) fx/2, GetY() + (int) fy/2)))
				return;

			// NOTE: When a monster wanders around in IDLE state it currently always runs. (Never walks)
			// The graphics team wanted to see monsters walking too, so temporarily walk or run with some probability. (Only active in test mode for now, since it changes the overall feel of the game)
			if (metin2::config::test_server())
			{
				if (number(0, 100) < 60)
					SetNowWalking(false);
				else
					SetNowWalking(true);
			}

			if (Goto(GetX() + (int) fx, GetY() + (int) fy))
				SendMovePacket(FUNC_WAIT, 0, 0, 0, 0);

			return;
		}
	}

	MonsterChat(MONSTER_CHAT_WAIT);
}
// END_OF_STATE_IDLE_REFACTORING

bool __CHARACTER_GotoNearTarget(CHARACTER * self, CHARACTER * victim)
{
	if (IS_SET(self->GetAIFlag(), AIFLAG_NOMOVE))
		return false;

	switch (self->GetMobBattleType())
	{
		case BATTLE_TYPE_RANGE:
		case BATTLE_TYPE_MAGIC:
			// Mages and archers move to 80% of their attack range before attacking.
			if (self->Follow(victim, self->GetMobAttackRange() * 8 / 10))
				return true;
			break;

		default:
			// The rest 90%?
			if (self->Follow(victim, self->GetMobAttackRange() * 9 / 10))
				return true;
	}

	return false;
}

void CHARACTER::StateMove()
{
	uint32_t dwElapsedTime = get_dword_time() - m_dwMoveStartTime;
	float fRate = (float) dwElapsedTime / (float) m_dwMoveDuration;

	if (fRate > 1.0f)
		fRate = 1.0f;

	int x = (int) ((float) (m_posDest.x - m_posStart.x) * fRate + m_posStart.x);
	int y = (int) ((float) (m_posDest.y - m_posStart.y) * fRate + m_posStart.y);

	Move(x, y);

	if (IsPC() && (thecore_pulse() & 15) == 0)
	{
		UpdateSectree();

		if (GetExchange())
		{
			CHARACTER * victim = GetExchange()->GetCompany()->GetOwner();
			int iDist = DISTANCE_APPROX(GetX() - victim->GetX(), GetY() - victim->GetY());

			// Distance check
			if (iDist >= EXCHANGE_MAX_DISTANCE)
			{
				GetExchange()->Cancel();
			}
		}
	}

	// Stamina must be above 0.
	if (IsPC())
	{
		if (IsWalking() && GetStamina() < GetMaxStamina())
		{
			// Stamina increases after 5 seconds
			if (get_dword_time() - GetWalkStartTime() > 5000)
				PointChange(POINT_STAMINA, GetMaxStamina() / 1);
		}

		// If in combat and running
		if (!IsWalking() && !IsRiding())
		{
			if ((get_dword_time() - GetLastAttackTime()) < 20000)
			{
				StartAffectEvent();

				if (IsStaminaHalfConsume())
				{
					if (thecore_pulse()&1)
						PointChange(POINT_STAMINA, -STAMINA_PER_STEP);
				}
				else
					PointChange(POINT_STAMINA, -STAMINA_PER_STEP);

				StartStaminaConsume();

				if (GetStamina() <= 0)
				{
					// Not enough stamina, must walk
					SetStamina(0);
					SetNowWalking(true);
					StopStaminaConsume();
				}
			}
			else if (IsStaminaConsume())
			{
				StopStaminaConsume();
			}
		}
	}
	else
	{
		// XXX AGGRO 
		if (IsMonster() && GetVictim())
		{
			CHARACTER * victim = GetVictim();
			UpdateAggrPoint(victim, DAMAGE_TYPE_NORMAL, -(victim->GetLevel() / 3 + 1));

			if (metin2::config::test_server())
			{
				// A monster chasing an enemy always runs.
				SetNowWalking(false);
			}
		}

		if (IsMonster() && GetMobRank() >= MOB_RANK_BOSS && GetVictim())
		{
			CHARACTER * victim = GetVictim();

			// Giant turtle
			if (GetRaceNum() == 2191 && number(1, 20) == 1 && get_dword_time() - m_pkMobInst->m_dwLastWarpTime > 1000)
			{
				// Warp test
				float fx, fy;
				GetDeltaByDegree(victim->GetRotation(), 400, &fx, &fy);
				int32_t new_x = victim->GetX() + (int32_t)fx;
				int32_t new_y = victim->GetY() + (int32_t)fy;
				SetRotation(GetDegreeFromPositionXY(new_x, new_y, victim->GetX(), victim->GetY()));
				Show(victim->GetMapIndex(), new_x, new_y, 0, true);
				GotoState(m_stateBattle);
				m_dwStateDuration = 1;
				ResetMobSkillCooltime();
				m_pkMobInst->m_dwLastWarpTime = get_dword_time();
				return;
			}

			// TODO Turn around to be less dumb!
			if (number(0, 3) == 0)
			{
				if (__CHARACTER_GotoNearTarget(this, victim))
					return;
			}
		}
	}

	if (1.0f == fRate)
	{
		if (IsPC())
		{
			metin2::log::debug( "도착 {} {:d} {:d}", GetName(), x, y);
			GotoState(m_stateIdle);
			StopStaminaConsume();
		}
		else
		{
			if (GetVictim() && !IsCoward())
			{
				if (!IsState(m_stateBattle))
					MonsterLog("[BATTLE] 근처에 왔으니 공격시작 %s", GetVictim()->GetName());

				GotoState(m_stateBattle);
				m_dwStateDuration = 1;
			}
			else
			{
				if (!IsState(m_stateIdle))
					MonsterLog("[IDLE] 대상이 없으니 쉬자");

				GotoState(m_stateIdle);

				m_dwStateDuration = PASSES_PER_SEC(number(1, 3));
			}
		}
	}
}

void CHARACTER::StateBattle()
{
	if (IsStone())
	{
		metin2::log::error("Stone must not use battle state (name {})", GetName());
		return;
	}

	if (IsPC())
		return; 

	if (!CanMove())
		return;

	if (IsStun())
		return;

	CHARACTER * victim = GetVictim();

	if (IsCoward())
	{
		if (IsDead())
			return;

		SetVictim(nullptr);

		if (number(1, 50) != 1)
		{
			GotoState(m_stateIdle);
			m_dwStateDuration = 1;
		}
		else
			CowardEscape();

		return;
	}

	if (!victim || (victim->IsStun() && IsGuardNPC()) || victim->IsDead())
	{
		if (victim && victim->IsDead() &&
				!metin2::config::game().no_wander && IsAggressive() && (!GetParty() || GetParty()->GetLeader() == this))
		{
			CHARACTER * new_victim = FindVictim(this, m_pkMobData->m_table.wAggressiveSight);

			SetVictim(new_victim);
			m_dwStateDuration = PASSES_PER_SEC(1);

			if (!new_victim)
			{
				switch (GetMobBattleType())
				{
					case BATTLE_TYPE_MELEE:
					case BATTLE_TYPE_SUPER_POWER:
					case BATTLE_TYPE_SUPER_TANKER:
					case BATTLE_TYPE_POWER:
					case BATTLE_TYPE_TANKER:
						{
							float fx, fy;
							float fDist = number(400, 1500);

							GetDeltaByDegree(number(0, 359), fDist, &fx, &fy);

							if (SECTREE_MANAGER::instance().IsMovablePosition(victim->GetMapIndex(),
										victim->GetX() + (int) fx, 
										victim->GetY() + (int) fy) && 
									SECTREE_MANAGER::instance().IsMovablePosition(victim->GetMapIndex(),
										victim->GetX() + (int) fx/2,
										victim->GetY() + (int) fy/2))
							{
								float dx = victim->GetX() + fx;
								float dy = victim->GetY() + fy;

								SetRotation(GetDegreeFromPosition(dx, dy));

								if (Goto((int32_t) dx, (int32_t) dy))
								{
									metin2::log::info( "KILL_AND_GO: {} distance {:.1f}", GetName(), fDist);
									SendMovePacket(FUNC_WAIT, 0, 0, 0, 0);
								}
							}
						}
				}
			}
			return;
		}

		SetVictim(nullptr);

		if (IsGuardNPC())
			Return();

		m_dwStateDuration = PASSES_PER_SEC(1);
		return;
	}

	if (IsSummonMonster() && !IsDead() && !IsStun())
	{
		if (!GetParty())
		{
			// Create a party to fill with summons.
			CPartyManager::instance().CreateParty(this);
		}

		CParty * pParty = GetParty();
		bool bPct = !number(0, 3);

		if (bPct && pParty->CountMemberByVnum(GetSummonVnum()) < SUMMON_MONSTER_COUNT)
		{
			MonsterLog("부하 몬스터 소환!");
			// Summon the missing ones to fill it up.
			int sx = GetX() - 300;
			int sy = GetY() - 300;
			int ex = GetX() + 300;
			int ey = GetY() + 300;

			CHARACTER * tch = CHARACTER_MANAGER::instance().SpawnMobRange(GetSummonVnum(), GetMapIndex(), sx, sy, ex, ey, true, true);

			if (tch)
			{
				pParty->Join(tch->GetVID());
				pParty->Link(tch);
			}
		}
	}

	CHARACTER * pkChrProtege = GetProtege();

	float fDist = DISTANCE_APPROX(GetX() - victim->GetX(), GetY() - victim->GetY());

	if (fDist >= 4000.0f)   // Give up if more than 40 meters away
	{
		MonsterLog("타겟이 멀어서 포기");
		SetVictim(nullptr);

		// Go near what we protect (stone, party leader).
		if (pkChrProtege)
			if (DISTANCE_APPROX(GetX() - pkChrProtege->GetX(), GetY() - pkChrProtege->GetY()) > 1000)
				Follow(pkChrProtege, number(150, 400));

		return;
	}

	if (fDist >= GetMobAttackRange() * 1.15)
	{
		__CHARACTER_GotoNearTarget(this, victim);
		return;
	}

	if (m_pkParty)
		m_pkParty->SendMessage(this, PM_ATTACKED_BY, 0, 0);

	if (2493 == m_pkMobData->m_table.dwVnum)
	{
		// Special handling for the Water Dragon (2493)
		m_dwStateDuration = BlueDragon_StateBattle(this);
		return;
	}

	uint32_t dwCurTime = get_dword_time();
	uint32_t dwDuration = CalculateDuration(GetLimitPoint(POINT_ATT_SPEED), 2000);

	if ((dwCurTime - m_dwLastAttackTime) < dwDuration) // Must attack every 2 seconds.
	{
		m_dwStateDuration = std::max<int>(1, (metin2::config::server().passes_per_sec * (dwDuration - (dwCurTime - m_dwLastAttackTime)) / 1000));
		return;
	}

	if (IsBerserker() == true)
		if (GetHPPct() < m_pkMobData->m_table.bBerserkPoint)
			if (IsBerserk() != true)
				SetBerserk(true);

	if (IsGodSpeeder() == true)
		if (GetHPPct() < m_pkMobData->m_table.bGodSpeedPoint)
			if (IsGodSpeed() != true)
				SetGodSpeed(true);

	//
	// Mob skill handling
	//
	if (HasMobSkill())
	{
		for (unsigned int iSkillIdx = 0; iSkillIdx < MOB_SKILL_MAX_NUM; ++iSkillIdx)
		{
			if (CanUseMobSkill(iSkillIdx))
			{
				SetRotationToXY(victim->GetX(), victim->GetY());

				if (UseMobSkill(iSkillIdx))
				{
					SendMovePacket(FUNC_MOB_SKILL, iSkillIdx, GetX(), GetY(), 0, dwCurTime);

					float fDuration = CMotionManager::instance().GetMotionDuration(GetRaceNum(), MAKE_MOTION_KEY(MOTION_MODE_GENERAL, MOTION_SPECIAL_1 + iSkillIdx));
					m_dwStateDuration = (uint32_t) (fDuration == 0.0f ? PASSES_PER_SEC(2) : PASSES_PER_SEC(fDuration));

					if (metin2::config::test_server())
						metin2::log::info( "USE_MOB_SKILL: {} idx {:d} motion {:d} duration {:.0f}", GetName(), iSkillIdx, MOTION_SPECIAL_1 + iSkillIdx, fDuration);

					return;
				}
			}
		}
	}

	if (!Attack(victim))    // Attack failed? Why did it fail? TODO
		m_dwStateDuration = metin2::config::server().passes_per_sec / 2;
	else
	{
		// Face the enemy.
		SetRotationToXY(victim->GetX(), victim->GetY());

		SendMovePacket(FUNC_ATTACK, 0, GetX(), GetY(), 0, dwCurTime);

		float fDuration = CMotionManager::instance().GetMotionDuration(GetRaceNum(), MAKE_MOTION_KEY(MOTION_MODE_GENERAL, MOTION_NORMAL_ATTACK));
		m_dwStateDuration = (uint32_t) (fDuration == 0.0f ? PASSES_PER_SEC(2) : PASSES_PER_SEC(fDuration));
	}
}

void CHARACTER::StateFlag()
{
	m_dwStateDuration = (uint32_t) PASSES_PER_SEC(0.5);

	CWarMap * pMap = GetWarMap();

	if (!pMap)
		return;

	FuncFindChrForFlag f(this);
	GetSectree()->ForEachAround(f);

	if (!f.m_pkChrFind)
		return;

	if (nullptr == f.m_pkChrFind->GetGuild())
		return;

	char buf[256];
	uint8_t idx;

	if (!pMap->GetTeamIndex(GetPoint(POINT_STAT), idx))
		return;

	f.m_pkChrFind->AddAffect(AFFECT_WAR_FLAG, POINT_NONE, GetPoint(POINT_STAT), idx == 0 ? AFF_WAR_FLAG1 : AFF_WAR_FLAG2, INFINITE_AFFECT_DURATION, 0, false);
	f.m_pkChrFind->AddAffect(AFFECT_WAR_FLAG, POINT_MOV_SPEED, 50 - f.m_pkChrFind->GetPoint(POINT_MOV_SPEED), 0, INFINITE_AFFECT_DURATION, 0, false);

	pMap->RemoveFlag(idx);

	snprintf(buf, sizeof(buf), LC_TEXT("%s 길드의 깃발을 %s 님이 획득하였습니다."), pMap->GetGuild(idx)->GetName(), f.m_pkChrFind->GetName());
	pMap->Notice(buf);
}

void CHARACTER::StateFlagBase()
{
	m_dwStateDuration = (uint32_t) PASSES_PER_SEC(0.5);

	FuncFindChrForFlagBase f(this);
	GetSectree()->ForEachAround(f);
}

void CHARACTER::StateHorse()
{
	float	START_FOLLOW_DISTANCE = 400.0f;		// Start following when farther than this distance
	float	START_RUN_DISTANCE = 700.0f;		// Run to follow when farther than this distance.
	int		MIN_APPROACH = 150;					// Minimum approach distance
	int		MAX_APPROACH = 300;					// Maximum approach distance	

	uint32_t	STATE_DURATION = (uint32_t)PASSES_PER_SEC(0.5);	// State duration

	bool bDoMoveAlone = true;					// Whether to wander around alone when near the character -_-;
	bool bRun = true;							// Should it run?

	if (IsDead())
		return;

	m_dwStateDuration = STATE_DURATION;

	CHARACTER * victim = GetRider();

	// not ! // If there is no target, the summoner will clear me directly
	if (!victim)
	{
		CHARACTER_MANAGER::instance().DestroyCharacter(this);
		return;
	}

	m_pkMobInst->m_posLastAttacked = GetXYZ();

	float fDist = DISTANCE_APPROX(GetX() - victim->GetX(), GetY() - victim->GetY());

	if (fDist >= START_FOLLOW_DISTANCE)
	{
		if (fDist > START_RUN_DISTANCE)
			SetNowWalking(!bRun);		// NOTE: From the function name I thought it stops, but SetNowWalking(false) means running.. -_-;

		Follow(victim, number(MIN_APPROACH, MAX_APPROACH));

		m_dwStateDuration = STATE_DURATION;
	}
	else if (bDoMoveAlone && (get_dword_time() > m_dwLastAttackTime))
	{
		// wondering-.-
		m_dwLastAttackTime = get_dword_time() + number(5000, 12000);

		SetRotation(number(0, 359));        // Pick a random direction

		float fx, fy;
		float fDist = number(200, 400);

		GetDeltaByDegree(GetRotation(), fDist, &fx, &fy);

		// Loose blocked-attribute check; don't go if the final or midpoint position is unreachable.
		if (!(SECTREE_MANAGER::instance().IsMovablePosition(GetMapIndex(), GetX() + (int) fx, GetY() + (int) fy) 
					&& SECTREE_MANAGER::instance().IsMovablePosition(GetMapIndex(), GetX() + (int) fx/2, GetY() + (int) fy/2)))
			return;

		SetNowWalking(true);

		if (Goto(GetX() + (int) fx, GetY() + (int) fy))
			SendMovePacket(FUNC_WAIT, 0, 0, 0, 0);
	}
}

