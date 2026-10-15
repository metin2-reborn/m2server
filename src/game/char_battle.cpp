#include "affect.h"
#include "arena.h"
#include "battle.h"
#include "BattleArena.h"
#include "BlueDragon.h"
#include "castle.h"
#include "char.h"
#include "char_manager.h"
#include "config.h"
#include "constants.h"
#include "cube.h"
#include "db.h"
#include "desc.h"
#include "DragonLair.h"
#include "dungeon.h"
#include "event.h"
#include "exchange.h"
#include "guild.h"
#include "guild_manager.h"
#include "item.h"
#include "item_manager.h"
#include "locale.hpp"
#include "log.h"
#include "marriage.h"
#include "mob_manager.h"
#include "monarch.h"
#include "packet.h"
#include "party.h"
#include "priv_manager.h"
#include "pvp.h"
#include "quest.h"
#include "questmanager.h"
#include "regen.h"
#include "sectree.h"
#include "sectree_manager.h"
#include "shop_manager.h"
#include "skill.h"
#include "start_position.h"
#include "threeway_war.h"
#include "typedef.h"
#include "unique_item.h"
#include "utils.h"
#include "vector.h"
#include "vid.h"

#include <common/length.h>
#include <metin2/log/log.h>
#include <libthecore/main.h>
#include <libthecore/utils.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <queue>
#include <random>
#include <utility>
#include <vector>

#include <unistd.h>

namespace metin2::game {

uint32_t AdjustExpByLevel(CHARACTER * const ch, const uint32_t exp)
{
	if (PLAYER_EXP_TABLE_MAX < ch->GetLevel())
	{
		double ret = 0.95;
		double factor = 0.1;

		for (ssize_t i=0 ; i < ch->GetLevel()-100 ; ++i)
		{
			if ( (i%10) == 0)
				factor /= 2.0;

			ret *= 1.0 - factor;
		}

		ret = ret * static_cast<double>(exp);

		if (ret < 1.0)
			return 1;

		return static_cast<uint32_t>(ret);
	}

	return exp;
}

bool CHARACTER::CanBeginFight() const
{
	if (!CanMove())
		return false;

	return m_pointsInstant.position == POS_STANDING && !IsDead() && !IsStun();
}

void CHARACTER::BeginFight(CHARACTER * pkVictim)
{
	SetVictim(pkVictim);
	SetPosition(POS_FIGHTING);
	SetNextStatePulse(1);
}

void CHARACTER::CreateFly(uint8_t bType, CHARACTER * pkVictim)
{
	TPacketGCCreateFly packFly;

	packFly.bHeader         = HEADER_GC_CREATE_FLY;
	packFly.bType           = bType;
	packFly.dwStartVID      = GetVID();
	packFly.dwEndVID        = pkVictim->GetVID();

	PacketAround(&packFly, sizeof(TPacketGCCreateFly));
}

void CHARACTER::DistributeSP(CHARACTER * pkKiller, int iMethod)
{
	if (pkKiller->GetSP() >= pkKiller->GetMaxSP())
		return;

	bool bAttacking = (get_dword_time() - GetLastAttackTime()) < 3000;
	bool bMoving = (get_dword_time() - GetLastMoveTime()) < 3000;

	if (iMethod == 1)
	{
		int num = number(0, 3);

		if (!num)
		{
			int iLvDelta = GetLevel() - pkKiller->GetLevel();
			int iAmount = 0;

			if (iLvDelta >= 5)
				iAmount = 10;
			else if (iLvDelta >= 0)
				iAmount = 6;
			else if (iLvDelta >= -3)
				iAmount = 2;

			if (iAmount != 0)
			{
				iAmount += (iAmount * pkKiller->GetPoint(POINT_SP_REGEN)) / 100;

				if (iAmount >= 11)
					CreateFly(FLY_SP_BIG, pkKiller);
				else if (iAmount >= 7)
					CreateFly(FLY_SP_MEDIUM, pkKiller);
				else
					CreateFly(FLY_SP_SMALL, pkKiller);

				pkKiller->PointChange(POINT_SP, iAmount);
			}
		}
	}
	else
	{
		if (pkKiller->GetJob() == JOB_SHAMAN || (pkKiller->GetJob() == JOB_SURA && pkKiller->GetSkillGroup() == 2))
		{
			int iAmount;

			if (bAttacking)
				iAmount = 2 + GetMaxSP() / 100;
			else if (bMoving)
				iAmount = 3 + GetMaxSP() * 2 / 100;
			else
				iAmount = 10 + GetMaxSP() * 3 / 100; // Normal state

			iAmount += (iAmount * pkKiller->GetPoint(POINT_SP_REGEN)) / 100;
			pkKiller->PointChange(POINT_SP, iAmount);
		}
		else
		{
			int iAmount;

			if (bAttacking)
				iAmount = 2 + pkKiller->GetMaxSP() / 200;
			else if (bMoving)
				iAmount = 2 + pkKiller->GetMaxSP() / 100;
			else
			{
				// Normal state
				if (pkKiller->GetHP() < pkKiller->GetMaxHP())
					iAmount = 2 + (pkKiller->GetMaxSP() / 100); // When HP is not full
				else
					iAmount = 9 + (pkKiller->GetMaxSP() / 100); // Default
			}

			iAmount += (iAmount * pkKiller->GetPoint(POINT_SP_REGEN)) / 100;
			pkKiller->PointChange(POINT_SP, iAmount);
		}
	}
}


bool CHARACTER::Attack(CHARACTER * pkVictim, uint8_t bType)
{
	if (metin2::config::test_server())
		metin2::log::info( "[TEST_SERVER] Attack : {} type {:d}, MobBattleType {:d}", GetName(), bType, !GetMobBattleType() ? 0 : GetMobAttackRange());
	if (!CanMove())
		return false;

	// CASTLE
	if (IS_CASTLE_MAP(GetMapIndex()) && false == castle_can_attack(this, pkVictim))
		return false;
	// CASTLE

	uint32_t dwCurrentTime = get_dword_time();

	if (IsPC())
	{
		if (IS_SPEED_HACK(this, pkVictim, dwCurrentTime))
			return false;

		if (bType == 0 && dwCurrentTime < GetSkipComboAttackByTime())
			return false;
	}
	else
	{
		MonsterChat(MONSTER_CHAT_ATTACK);
	}

	pkVictim->SetSyncOwner(this);

	if (pkVictim->CanBeginFight())
		pkVictim->BeginFight(this);

	int iRet;

	if (bType == 0)
	{
		//
		// Normal attack
		//
		switch (GetMobBattleType())
		{
			case BATTLE_TYPE_MELEE:
			case BATTLE_TYPE_POWER:
			case BATTLE_TYPE_TANKER:
			case BATTLE_TYPE_SUPER_POWER:
			case BATTLE_TYPE_SUPER_TANKER:
				iRet = battle_melee_attack(this, pkVictim);
				break;

			case BATTLE_TYPE_RANGE:
				FlyTarget(pkVictim->GetVID(), pkVictim->GetX(), pkVictim->GetY(), HEADER_CG_FLY_TARGETING);
				iRet = Shoot(0) ? BATTLE_DAMAGE : BATTLE_NONE;
				break;

			case BATTLE_TYPE_MAGIC:
				FlyTarget(pkVictim->GetVID(), pkVictim->GetX(), pkVictim->GetY(), HEADER_CG_FLY_TARGETING);
				iRet = Shoot(1) ? BATTLE_DAMAGE : BATTLE_NONE;
				break;

			default:
				metin2::log::error("Unhandled battle type {:d}", GetMobBattleType());
				iRet = BATTLE_NONE;
				break;
		}
	}
	else
	{
		if (IsPC() == true)
		{
			if (dwCurrentTime - m_dwLastSkillTime > 1500)
			{
				metin2::log::debug( "HACK: Too long skill using term. Name({}) PID({:d}) delta({:d})",
						GetName(), GetPlayerID(), (dwCurrentTime - m_dwLastSkillTime));
				return false;
			}
		}

		metin2::log::debug( "Attack call ComputeSkill {:d} {}", bType, pkVictim?pkVictim->GetName():"");
		iRet = ComputeSkill(bType, pkVictim);
	}

	if (iRet == BATTLE_DAMAGE || iRet == BATTLE_DEAD)
	{
		OnMove(true);
		pkVictim->OnMove();

		// only pc sets victim null. For npc, state machine will reset this.
		if (BATTLE_DEAD == iRet && IsPC())
			SetVictim(nullptr);

		return true;
	}

	return false;
}

void CHARACTER::DeathPenalty(uint8_t bTown)
{
	metin2::log::debug( "DEATH_PERNALY_CHECK({}) town({:d})", GetName(), bTown);

	Cube_close(this);

	if (CBattleArena::instance().IsBattleArenaMap(GetMapIndex()) == true)
	{
		return;
	}
	
	if (GetLevel() < 10)
	{
		metin2::log::info( "NO_DEATH_PENALTY_LESS_LV10({})", GetName());
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("용신의 가호로 경험치가 떨어지지 않았습니다."));
		return;
	}

   	if (number(0, 2))
	{
		metin2::log::info( "NO_DEATH_PENALTY_LUCK({})", GetName());
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("용신의 가호로 경험치가 떨어지지 않았습니다."));
		return;
	}

	if (IS_SET(m_pointsInstant.instant_flag, INSTANT_FLAG_DEATH_PENALTY))
	{
		REMOVE_BIT(m_pointsInstant.instant_flag, INSTANT_FLAG_DEATH_PENALTY);

		// NO_DEATH_PENALTY_BUG_FIX 
		if (!bTown) // In the international version, Dragon God's Protection is used only when reviving in place. (No experience penalty when returning to town)
		{
			if (FindAffect(AFFECT_NO_DEATH_PENALTY))
			{
				metin2::log::info( "NO_DEATH_PENALTY_AFFECT({})", GetName());
				ChatPacket(CHAT_TYPE_INFO, LC_TEXT("용신의 가호로 경험치가 떨어지지 않았습니다."));
				RemoveAffect(AFFECT_NO_DEATH_PENALTY);
				return;
			}
		}
		// END_OF_NO_DEATH_PENALTY_BUG_FIX

		int iLoss = ((GetNextExp() * aiExpLossPercents[std::clamp(GetLevel(), 1, PLAYER_EXP_TABLE_MAX)]) / 100);

		iLoss = std::min(800000, iLoss);

		if (bTown)
		{
			iLoss = 0;
		}

		if (IsEquipUniqueItem(UNIQUE_ITEM_TEARDROP_OF_GODNESS))
			iLoss /= 2;

		metin2::log::info( "DEATH_PENALTY({}) EXP_LOSS: {:d} percent {:d}%", GetName(), iLoss, aiExpLossPercents[std::min(metin2::config::game().max_level, GetLevel())]);

		PointChange(POINT_EXP, -iLoss, true);
	}
}

bool CHARACTER::IsStun() const
{
	if (IS_SET(m_pointsInstant.instant_flag, INSTANT_FLAG_STUN))
		return true;

	return false;
}

EVENTFUNC(StunEvent)
{
	char_event_info* info = dynamic_cast<char_event_info*>( event->info );

	if ( info == nullptr )
	{
		metin2::log::error( "StunEvent> <Factor> Null pointer" );
		return 0;
	}

	CHARACTER * ch = info->ch;

	if (ch == nullptr) { // <Factor>
		return 0;
	}
	ch->m_pkStunEvent = nullptr;
	ch->Dead();
	return 0;
}

void CHARACTER::Stun()
{
	if (IsStun())
		return;

	if (IsDead())
		return;

	if (!IsPC() && m_pkParty)
	{
		m_pkParty->SendMessage(this, PM_ATTACKED_BY, 0, 0);
	}

	metin2::log::debug( "{}: Stun {}", GetName(), static_cast<const void *>(this));

	PointChange(POINT_HP_RECOVERY, -GetPoint(POINT_HP_RECOVERY));
	PointChange(POINT_SP_RECOVERY, -GetPoint(POINT_SP_RECOVERY));

	CloseMyShop();

	event_cancel(&m_pkRecoveryEvent); // Kill the recovery event.

	TPacketGCStun pack;
	pack.header	= HEADER_GC_STUN;
	pack.vid	= m_vid;
	PacketAround(&pack, sizeof(pack));

	SET_BIT(m_pointsInstant.instant_flag, INSTANT_FLAG_STUN);

	if (m_pkStunEvent)
		return;

	char_event_info* info = AllocEventInfo<char_event_info>();

	info->ch = this;

	m_pkStunEvent = event_create(StunEvent, info, PASSES_PER_SEC(3));
}

EVENTINFO(SCharDeadEventInfo)
{
	bool isPC;
	uint32_t dwID;

	SCharDeadEventInfo()
	: isPC(0)
	, dwID(0)
	{
	}
};

EVENTFUNC(dead_event)
{
	const SCharDeadEventInfo* info = dynamic_cast<SCharDeadEventInfo*>(event->info);

	if ( info == nullptr )
	{
		metin2::log::error( "dead_event> <Factor> Null pointer" );
		return 0;
	}

	CHARACTER * ch = nullptr;

	if (true == info->isPC)
	{
		ch = CHARACTER_MANAGER::instance().FindByPID( info->dwID );
	}
	else
	{
		ch = CHARACTER_MANAGER::instance().Find( info->dwID );
	}

	if (nullptr == ch)
	{
		metin2::log::error("DEAD_EVENT: cannot find char pointer with {} id({:d})", info->isPC ? "PC" : "MOB", info->dwID );
		return 0;
	}

	ch->m_pkDeadEvent = nullptr;

	if (ch->GetDesc())
	{
		ch->GetDesc()->SetPhase(PHASE_GAME);

		ch->SetPosition(POS_STANDING);

		PIXEL_POSITION pos;

		if (SECTREE_MANAGER::instance().GetRecallPositionByEmpire(ch->GetMapIndex(), ch->GetEmpire(), pos))
			ch->WarpSet(pos.x, pos.y);
		else
		{
			metin2::log::error("cannot find spawn position (name {})", ch->GetName());
			ch->WarpSet(EMPIRE_START_X(ch->GetEmpire()), EMPIRE_START_Y(ch->GetEmpire()));
		}

		ch->PointChange(POINT_HP, (ch->GetMaxHP() / 2) - ch->GetHP(), true);

		ch->DeathPenalty(0);

		ch->StartRecoveryEvent();

		ch->ChatPacket(CHAT_TYPE_COMMAND, "CloseRestartWindow");
	}
	else
	{
		if (ch->IsMonster() == true)
		{
			if (ch->IsRevive() == false && ch->HasReviverInParty() == true)
			{
				ch->SetPosition(POS_STANDING);
				ch->SetHP(ch->GetMaxHP());

				ch->ViewReencode();

				ch->SetAggressive();
				ch->SetRevive(true);

				return 0;
			}
		}

		CHARACTER_MANAGER::instance().DestroyCharacter(ch);
	}

	return 0;
}

bool CHARACTER::IsDead() const
{
	if (m_pointsInstant.position == POS_DEAD)
		return true;

	return false;
}

static int GetGoldMultipler() { return metin2::config::game().distribution_test_server ? 3 : 1; }

void CHARACTER::RewardGold(CHARACTER * pkAttacker)
{
	// ADD_PREMIUM
	bool isAutoLoot = 
		(pkAttacker->GetPremiumRemainSeconds(PREMIUM_AUTOLOOT) > 0 ||
		 pkAttacker->IsEquipUniqueGroup(UNIQUE_GROUP_AUTOLOOT))
		? true : false; // Third Hand
	// END_OF_ADD_PREMIUM

	PIXEL_POSITION pos;

	if (!isAutoLoot)
		if (!SECTREE_MANAGER::instance().GetMovablePosition(GetMapIndex(), GetX(), GetY(), pos))
			return;

	int iTotalGold = 0;
	//
	// --------- Gold drop chance calculation ----------
	//
	int iGoldPercent = MobRankStats[GetMobRank()].iGoldPercent;

	if (pkAttacker->IsPC())
		iGoldPercent = iGoldPercent * (100 + CPrivManager::instance().GetPriv(pkAttacker, PRIV_GOLD_DROP)) / 100;

	if (pkAttacker->GetPoint(POINT_MALL_GOLDBONUS))
		iGoldPercent += (iGoldPercent * pkAttacker->GetPoint(POINT_MALL_GOLDBONUS) / 100);

	iGoldPercent = iGoldPercent * CHARACTER_MANAGER::instance().GetMobGoldDropRate(pkAttacker) / 100;

	// ADD_PREMIUM
	if (pkAttacker->GetPremiumRemainSeconds(PREMIUM_GOLD) > 0 ||
			pkAttacker->IsEquipUniqueGroup(UNIQUE_GROUP_LUCKY_GOLD))
		iGoldPercent += iGoldPercent;
	// END_OF_ADD_PREMIUM

	if (iGoldPercent > 100) 
		iGoldPercent = 100;

	int iPercent;

	if (GetMobRank() >= MOB_RANK_BOSS)
		iPercent = ((iGoldPercent * PERCENT_LVDELTA_BOSS(pkAttacker->GetLevel(), GetLevel())) / 100);
	else
		iPercent = ((iGoldPercent * PERCENT_LVDELTA(pkAttacker->GetLevel(), GetLevel())) / 100);

	if (number(1, 100) > iPercent)
		return;

	int iGoldMultipler = GetGoldMultipler();

	if (1 == number(1, 50000)) // 1/50000 chance of 10x gold
		iGoldMultipler *= 10;
	else if (1 == number(1, 10000)) // 1/10000 chance of 5x gold
		iGoldMultipler *= 5;

	// Personal modifier
	if (pkAttacker->GetPoint(POINT_GOLD_DOUBLE_BONUS))
		if (number(1, 100) <= pkAttacker->GetPoint(POINT_GOLD_DOUBLE_BONUS))
			iGoldMultipler *= 2;

	//
	// --------- Determine gold drop multiplier ----------
	//
	if (metin2::config::test_server())
		pkAttacker->ChatPacket(CHAT_TYPE_PARTY, "gold_mul %d rate %d", iGoldMultipler, CHARACTER_MANAGER::instance().GetMobGoldAmountRate(pkAttacker));

	//
	// --------- Actual drop handling -------------
	// 
	CItem * item;

	int iGold10DropPct = 100;
	iGold10DropPct = (iGold10DropPct * 100) / (100 + CPrivManager::instance().GetPriv(pkAttacker, PRIV_GOLD10_DROP));

	// If MOB_RANK is higher than BOSS, always a gold bomb
	if (GetMobRank() >= MOB_RANK_BOSS && !IsStone() && GetMobTable().dwGoldMax != 0)
	{
		if (1 == number(1, iGold10DropPct))
			iGoldMultipler *= 10; // 1% chance of 10x gold

		int iSplitCount = number(25, 35);

		for (int i = 0; i < iSplitCount; ++i)
		{
			int iGold = number(GetMobTable().dwGoldMin, GetMobTable().dwGoldMax) / iSplitCount;
			if (metin2::config::test_server())
				metin2::log::info( "iGold {:d}", iGold);
			iGold = iGold * CHARACTER_MANAGER::instance().GetMobGoldAmountRate(pkAttacker) / 100;
			iGold *= iGoldMultipler;

			if (iGold == 0)
			{
				continue ;
			}

			if (metin2::config::test_server())
			{
				metin2::log::info( "Drop Moeny MobGoldAmountRate {:d} {:d}", CHARACTER_MANAGER::instance().GetMobGoldAmountRate(pkAttacker), iGoldMultipler);
				metin2::log::info( "Drop Money gold {:d} GoldMin {:d} GoldMax {:d}", iGold, GetMobTable().dwGoldMax, GetMobTable().dwGoldMax);
			}

			// NOTE: gold bombs do not go through Third Hand handling
			if ((item = ITEM_MANAGER::instance().CreateItem(1, iGold)))
			{
				pos.x = GetX() + ((number(-14, 14) + number(-14, 14)) * 23);
				pos.y = GetY() + ((number(-14, 14) + number(-14, 14)) * 23);

				item->AddToGround(GetMapIndex(), pos);
				item->StartDestroyEvent();

				iTotalGold += iGold; // Total gold
			}
		}
	}
	// 1% chance to drop 10 piles of gold. (10x drop)
	else if (1 == number(1, iGold10DropPct))
	{
		//
		// Gold-bomb style drop
		//
		for (int i = 0; i < 10; ++i)
		{
			int iGold = number(GetMobTable().dwGoldMin, GetMobTable().dwGoldMax);
			iGold = iGold * CHARACTER_MANAGER::instance().GetMobGoldAmountRate(pkAttacker) / 100;
			iGold *= iGoldMultipler;

			if (iGold == 0)
			{
				continue;
			}

			// NOTE: gold bombs do not go through Third Hand handling
			if ((item = ITEM_MANAGER::instance().CreateItem(1, iGold)))
			{
				pos.x = GetX() + (number(-7, 7) * 20);
				pos.y = GetY() + (number(-7, 7) * 20);

				item->AddToGround(GetMapIndex(), pos);
				item->StartDestroyEvent();

				iTotalGold += iGold; // Total gold
			}
		}
	}
	else
	{
		//
		// Normal gold drop
		//
		int iGold = number(GetMobTable().dwGoldMin, GetMobTable().dwGoldMax);
		iGold = iGold * CHARACTER_MANAGER::instance().GetMobGoldAmountRate(pkAttacker) / 100;
		iGold *= iGoldMultipler;

		int iSplitCount;

		if (iGold >= 3) 
			iSplitCount = number(1, 3);
		else if (GetMobRank() >= MOB_RANK_BOSS)
		{
			iSplitCount = number(3, 10);

			if ((iGold / iSplitCount) == 0)
				iSplitCount = 1;
		}
		else
			iSplitCount = 1;

		if (iGold != 0)
		{
			iTotalGold += iGold; // Total gold

			for (int i = 0; i < iSplitCount; ++i)
			{
				if (isAutoLoot)
				{
					pkAttacker->GiveGold(iGold / iSplitCount);
				}
				else if ((item = ITEM_MANAGER::instance().CreateItem(1, iGold / iSplitCount)))
				{
					pos.x = GetX() + (number(-7, 7) * 20);
					pos.y = GetY() + (number(-7, 7) * 20);

					item->AddToGround(GetMapIndex(), pos);
					item->StartDestroyEvent();
				}
			}
		}
	}

	DBManager::instance().SendMoneyLog(MONEY_LOG_MONSTER, GetRaceNum(), iTotalGold);
}

void CHARACTER::Reward(bool bItemDrop)
{
	if (GetRaceNum() == 5001) // Pirates (Waegu) always drop gold
	{
		PIXEL_POSITION pos;

		if (!SECTREE_MANAGER::instance().GetMovablePosition(GetMapIndex(), GetX(), GetY(), pos))
			return;

		CItem * item;
		int iGold = number(GetMobTable().dwGoldMin, GetMobTable().dwGoldMax);
		iGold = iGold * CHARACTER_MANAGER::instance().GetMobGoldAmountRate(nullptr) / 100;
		iGold *= GetGoldMultipler();
		int iSplitCount = number(25, 35);

		metin2::log::info( "WAEGU Dead gold {:d} split {:d}", iGold, iSplitCount);

		for (int i = 1; i <= iSplitCount; ++i)
		{
			if ((item = ITEM_MANAGER::instance().CreateItem(1, iGold / iSplitCount)))
			{
				if (i != 0)
				{
					pos.x = number(-7, 7) * 20;
					pos.y = number(-7, 7) * 20;

					pos.x += GetX();
					pos.y += GetY();
				}

				item->AddToGround(GetMapIndex(), pos);
				item->StartDestroyEvent();
			}
		}
		return;
	}

   	CHARACTER * pkAttacker = DistributeExp();

	if (!pkAttacker)
		return;

	if (pkAttacker->IsPC())
	{
		if (GetLevel() - pkAttacker->GetLevel() >= -10)
		{
			if (pkAttacker->GetRealAlignment() < 0)
			{
				if (pkAttacker->IsEquipUniqueItem(UNIQUE_ITEM_FASTER_ALIGNMENT_UP_BY_KILL))
					pkAttacker->UpdateAlignment(14);
				else
					pkAttacker->UpdateAlignment(7);
			}
			else
				pkAttacker->UpdateAlignment(2);
		}

		pkAttacker->SetQuestNPCID(GetVID());
		quest::CQuestManager::instance().Kill(pkAttacker->GetPlayerID(), GetRaceNum());
		CHARACTER_MANAGER::instance().KillLog(GetRaceNum());

		if (!number(0, 9))
		{
			if (pkAttacker->GetPoint(POINT_KILL_HP_RECOVERY))
			{
				int iHP = pkAttacker->GetMaxHP() * pkAttacker->GetPoint(POINT_KILL_HP_RECOVERY) / 100;
				pkAttacker->PointChange(POINT_HP, iHP);
				CreateFly(FLY_HP_SMALL, pkAttacker);
			}

			if (pkAttacker->GetPoint(POINT_KILL_SP_RECOVER))
			{
				int iSP = pkAttacker->GetMaxSP() * pkAttacker->GetPoint(POINT_KILL_SP_RECOVER) / 100;
				pkAttacker->PointChange(POINT_SP, iSP);
				CreateFly(FLY_SP_SMALL, pkAttacker);
			}
		}
	}

	if (!bItemDrop)
		return;

	PIXEL_POSITION pos = GetXYZ();

	if (!SECTREE_MANAGER::instance().GetMovablePosition(GetMapIndex(), pos.x, pos.y, pos))
		return;

	//
	// Gold drop
	//
	if (metin2::config::test_server())
		metin2::log::info( "Drop money : Attacker {}", pkAttacker->GetName());
	RewardGold(pkAttacker);

	//
	// Item drop
	//
	CItem * item;

	static std::vector<CItem *> s_vec_item;
	s_vec_item.clear();

	if (ITEM_MANAGER::instance().CreateDropItem(this, pkAttacker, s_vec_item))
	{
		if (s_vec_item.size() == 0);
		else if (s_vec_item.size() == 1)
		{
			item = s_vec_item[0];
			item->AddToGround(GetMapIndex(), pos);

			if (CBattleArena::instance().IsBattleArenaMap(pkAttacker->GetMapIndex()) == false)
			{
				item->SetOwnership(pkAttacker);
			}

			item->StartDestroyEvent();

			pos.x = number(-7, 7) * 20;
			pos.y = number(-7, 7) * 20;
			pos.x += GetX();
			pos.y += GetY();

			metin2::log::info( "DROP_ITEM: {} {:d} {:d} from {}", item->GetName(), pos.x, pos.y, GetName());
		}
		else
		{
			int iItemIdx = s_vec_item.size() - 1;

			std::priority_queue<std::pair<int, CHARACTER *> > pq;

			int total_dam = 0;

			for (TDamageMap::iterator it = m_map_kDamage.begin(); it != m_map_kDamage.end(); ++it)
			{
				int iDamage = it->second.iTotalDamage;
				if (iDamage > 0)
				{
					CHARACTER * ch = CHARACTER_MANAGER::instance().Find(it->first);

					if (ch)
					{
						pq.push(std::make_pair(iDamage, ch));
						total_dam += iDamage;
					}
				}
			}

			std::vector<CHARACTER *> v;

			while (!pq.empty() && pq.top().first * 10 >= total_dam)
			{
				v.push_back(pq.top().second);
				pq.pop();
			}

			if (v.empty())
			{
				// Nobody dealt notably more damage, so no ownership
				while (iItemIdx >= 0)
				{
					item = s_vec_item[iItemIdx--];

					if (!item)
					{
						metin2::log::error("item null in vector idx {:d}", iItemIdx + 1);
						continue;
					}

					item->AddToGround(GetMapIndex(), pos);
					// No ownership among those who dealt 10% damage or less
					item->StartDestroyEvent();

					pos.x = number(-7, 7) * 20;
					pos.y = number(-7, 7) * 20;
					pos.x += GetX();
					pos.y += GetY();

					metin2::log::info( "DROP_ITEM: {} {:d} {:d} by {}", item->GetName(), pos.x, pos.y, GetName());
				}
			}
			else
			{
				// Ownership is shared only among those who dealt a lot of damage
				std::vector<CHARACTER *>::iterator it = v.begin();

				while (iItemIdx >= 0)
				{
					item = s_vec_item[iItemIdx--];

					if (!item)
					{
						metin2::log::error("item null in vector idx {:d}", iItemIdx + 1);
						continue;
					}

					item->AddToGround(GetMapIndex(), pos);

					CHARACTER * ch = *it;

					if (ch->GetParty())
						ch = ch->GetParty()->GetNextOwnership(ch, GetX(), GetY());

					++it;

					if (it == v.end())
						it = v.begin();

					if (CBattleArena::instance().IsBattleArenaMap(ch->GetMapIndex()) == false)
					{
						item->SetOwnership(ch);
					}

					item->StartDestroyEvent();

					pos.x = number(-7, 7) * 20;
					pos.y = number(-7, 7) * 20;
					pos.x += GetX();
					pos.y += GetY();

					metin2::log::info( "DROP_ITEM: {} {:d} {:d} by {}", item->GetName(), pos.x, pos.y, GetName());
				}
			}
		}
	}

	m_map_kDamage.clear();
}

struct TItemDropPenalty
{
	int iInventoryPct;		// Range: 1 ~ 1000
	int iInventoryQty;		// Range: --
	int iEquipmentPct;		// Range: 1 ~ 100
	int iEquipmentQty;		// Range: --
};

TItemDropPenalty aItemDropPenalty_kor[9] =
{
	{   0,   0,  0,  0 },	// Noble King
	{   0,   0,  0,  0 },	// Hero
	{   0,   0,  0,  0 },	// Saint
	{   0,   0,  0,  0 },	// Wise
	{   0,   0,  0,  0 },	// Citizen
	{  25,   1,  5,  1 },	// Ronin
	{  50,   2, 10,  1 },	// Villain
	{  75,   4, 15,  1 },	// Demon Lord
	{ 100,   8, 20,  1 },	// Tyrant
};

void CHARACTER::ItemDropPenalty(CHARACTER * pkKiller)
{
	// Do not drop items while a private shop is open.
	if (GetMyShop())
		return;

	if (GetLevel() < 50)
		return;
	
	if (CBattleArena::instance().IsBattleArenaMap(GetMapIndex()) == true)
	{
		return;
	}

	struct TItemDropPenalty * table = &aItemDropPenalty_kor[0];

	if (GetLevel() < 10)
		return;

	int iAlignIndex;

	if (GetRealAlignment() >= 120000)
		iAlignIndex = 0;
	else if (GetRealAlignment() >= 80000)
		iAlignIndex = 1;
	else if (GetRealAlignment() >= 40000)
		iAlignIndex = 2;
	else if (GetRealAlignment() >= 10000)
		iAlignIndex = 3;
	else if (GetRealAlignment() >= 0)
		iAlignIndex = 4;
	else if (GetRealAlignment() > -40000)
		iAlignIndex = 5;
	else if (GetRealAlignment() > -80000)
		iAlignIndex = 6;
	else if (GetRealAlignment() > -120000)
		iAlignIndex = 7;
	else
		iAlignIndex = 8;

	std::vector<std::pair<CItem *, int> > vec_item;
	CItem * pkItem;
	int	i;
	bool isDropAllEquipments = false;

	TItemDropPenalty & r = table[iAlignIndex];
	metin2::log::info( "{} align {:d} inven_pct {:d} equip_pct {:d}", GetName(), iAlignIndex, r.iInventoryPct, r.iEquipmentPct);

	bool bDropInventory = r.iInventoryPct >= number(1, 1000);
	bool bDropEquipment = r.iEquipmentPct >= number(1, 100);
	bool bDropAntiDropUniqueItem = false;

	if ((bDropInventory || bDropEquipment) && IsEquipUniqueItem(UNIQUE_ITEM_SKIP_ITEM_DROP_PENALTY))
	{
		bDropInventory = false;
		bDropEquipment = false;
		bDropAntiDropUniqueItem = true;
	}

	if (bDropInventory) // Drop Inventory
	{
		std::vector<uint8_t> vec_bSlots;

		for (i = 0; i < INVENTORY_MAX_NUM; ++i)
			if (GetInventoryItem(i))
				vec_bSlots.push_back(i);

		if (!vec_bSlots.empty())
		{
			std::random_device rd;
			std::mt19937 g(rd());
			std::shuffle(vec_bSlots.begin(), vec_bSlots.end(), g);

			int iQty = std::min<int>(vec_bSlots.size(), r.iInventoryQty);

			if (iQty)
				iQty = number(1, iQty);

			for (i = 0; i < iQty; ++i)
			{
				pkItem = GetInventoryItem(vec_bSlots[i]);

				if (IS_SET(pkItem->GetAntiFlag(), ITEM_ANTIFLAG_GIVE | ITEM_ANTIFLAG_PKDROP))
					continue;

				SyncQuickslot(QUICKSLOT_TYPE_ITEM, vec_bSlots[i], 255);
				vec_item.push_back(std::make_pair(pkItem->RemoveFromCharacter(), INVENTORY));
			}
		}
		else if (iAlignIndex == 8)
			isDropAllEquipments = true;
	}

	if (bDropEquipment) // Drop Equipment
	{
		std::vector<uint8_t> vec_bSlots;

		for (i = 0; i < WEAR_MAX_NUM; ++i)
			if (GetWear(i))
				vec_bSlots.push_back(i);

		if (!vec_bSlots.empty())
		{
			std::random_device rd;
			std::mt19937 g(rd());
			std::shuffle(vec_bSlots.begin(), vec_bSlots.end(), g);
			int iQty;

			if (isDropAllEquipments)
				iQty = vec_bSlots.size();
			else
				iQty = std::min<int>(vec_bSlots.size(), number(1, r.iEquipmentQty));

			if (iQty)
				iQty = number(1, iQty);

			for (i = 0; i < iQty; ++i)
			{
				pkItem = GetWear(vec_bSlots[i]);

				if (IS_SET(pkItem->GetAntiFlag(), ITEM_ANTIFLAG_GIVE | ITEM_ANTIFLAG_PKDROP))
					continue;

				SyncQuickslot(QUICKSLOT_TYPE_ITEM, vec_bSlots[i], 255);
				vec_item.push_back(std::make_pair(pkItem->RemoveFromCharacter(), EQUIPMENT));
			}
		}
	}

	if (bDropAntiDropUniqueItem)
	{
		CItem * pkItem;

		pkItem = GetWear(WEAR_UNIQUE1);

		if (pkItem && pkItem->GetVnum() == UNIQUE_ITEM_SKIP_ITEM_DROP_PENALTY)
		{
			SyncQuickslot(QUICKSLOT_TYPE_ITEM, WEAR_UNIQUE1, 255);
			vec_item.push_back(std::make_pair(pkItem->RemoveFromCharacter(), EQUIPMENT));
		}

		pkItem = GetWear(WEAR_UNIQUE2);

		if (pkItem && pkItem->GetVnum() == UNIQUE_ITEM_SKIP_ITEM_DROP_PENALTY)
		{
			SyncQuickslot(QUICKSLOT_TYPE_ITEM, WEAR_UNIQUE2, 255);
			vec_item.push_back(std::make_pair(pkItem->RemoveFromCharacter(), EQUIPMENT));
		}
	}

	{
		PIXEL_POSITION pos;
		pos.x = GetX();
		pos.y = GetY();

		unsigned int i;

		for (i = 0; i < vec_item.size(); ++i)
		{
			CItem * item = vec_item[i].first;
			int window = vec_item[i].second;

			item->AddToGround(GetMapIndex(), pos);
			item->StartDestroyEvent();

			metin2::log::info( "DROP_ITEM_PK: {} {:d} {:d} from {}", item->GetName(), pos.x, pos.y, GetName());
			LogManager::instance().ItemLog(this, item, "DEAD_DROP", (window == INVENTORY) ? "INVENTORY" : ((window == EQUIPMENT) ? "EQUIPMENT" : ""));

			pos.x = GetX() + number(-7, 7) * 20;
			pos.y = GetY() + number(-7, 7) * 20;
		}
	}
}

class FPartyAlignmentCompute
{
	public:
		FPartyAlignmentCompute(int iAmount, int x, int y)
		{
			m_iAmount = iAmount;
			m_iCount = 0;
			m_iStep = 0;
			m_iKillerX = x;
			m_iKillerY = y;
		}

		void operator () (CHARACTER * pkChr)
		{
			if (DISTANCE_APPROX(pkChr->GetX() - m_iKillerX, pkChr->GetY() - m_iKillerY) < PARTY_DEFAULT_RANGE)
			{
				if (m_iStep == 0)
				{
					++m_iCount;
				}
				else
				{
					pkChr->UpdateAlignment(m_iAmount / m_iCount);
				}
			}
		}

		int m_iAmount;
		int m_iCount;
		int m_iStep;

		int m_iKillerX;
		int m_iKillerY;
};



void CHARACTER::Dead(CHARACTER * pkKiller, bool bImmediateDead)
{
	if (IsDead())
		return;

	{
		if (IsHorseRiding())
		{
			StopRiding();
		}
		else if (GetMountVnum())
		{
			RemoveAffect(AFFECT_MOUNT_BONUS);
			m_dwMountVnum = 0;
			UnEquipSpecialRideUniqueItem();

			UpdatePacket();
		}
	}

	if (!pkKiller && m_dwKillerPID)
		pkKiller = CHARACTER_MANAGER::instance().FindByPID(m_dwKillerPID);

	m_dwKillerPID = 0; // Must be initialized DO NOT DELETE THIS LINE UNLESS YOU ARE 1000000% SURE

	bool isAgreedPVP = false;
	bool isUnderGuildWar = false;
	bool isDuel = false;
	bool isForked = false;

	if (pkKiller && pkKiller->IsPC())
	{
		if (pkKiller->m_pkChrTarget == this)
			pkKiller->SetTarget(nullptr);

		if (!IsPC() && pkKiller->GetDungeon())
			pkKiller->GetDungeon()->IncKillCount(pkKiller, this);

		isAgreedPVP = CPVPManager::instance().Dead(this, pkKiller->GetPlayerID());
		isDuel = CArenaManager::instance().OnDead(pkKiller, this);

		if (IsPC())
		{
			CGuild * g1 = GetGuild();
			CGuild * g2 = pkKiller->GetGuild();

			if (g1 && g2)
				if (g1->UnderWar(g2->GetID()))
					isUnderGuildWar = true;

			pkKiller->SetQuestNPCID(GetVID());
			quest::CQuestManager::instance().Kill(pkKiller->GetPlayerID(), quest::QUEST_NO_NPC);
			CGuildManager::instance().Kill(pkKiller, this);
		}
	}

	//CHECK_FORKEDROAD_WAR
	if (IsPC())
	{
		if (CThreeWayWar::instance().IsThreeWayWarMapIndex(GetMapIndex()))
			isForked = true;
	}
	//END_CHECK_FORKEDROAD_WAR

	if (pkKiller &&
			!isAgreedPVP &&
			!isUnderGuildWar &&
			IsPC() &&
			!isDuel &&
			!isForked &&
			!IS_CASTLE_MAP(GetMapIndex()))
	{
		if (GetGMLevel() == GM_PLAYER || metin2::config::test_server())
		{
			ItemDropPenalty(pkKiller);
		}
	}

	// CASTLE_SIEGE
	if (IS_CASTLE_MAP(GetMapIndex()))
	{
		if (CASTLE_FROG_VNUM == GetRaceNum())
			castle_frog_die(this, pkKiller);
		else if (castle_is_guard_vnum(GetRaceNum()))
			castle_guard_die(this, pkKiller);
		else if (castle_is_tower_vnum(GetRaceNum()))
			castle_tower_die(this, pkKiller);
	}
	// CASTLE_SIEGE

	if (true == isForked)
	{
		CThreeWayWar::instance().onDead( this, pkKiller );
	}

	SetPosition(POS_DEAD);
	ClearAffect(true);

	if (pkKiller && IsPC())
	{
		if (!pkKiller->IsPC())
		{
			if (!isForked)
			{
				metin2::log::debug( "DEAD: {} {} WITH PENALTY", GetName(), static_cast<const void *>(this));
				SET_BIT(m_pointsInstant.instant_flag, INSTANT_FLAG_DEATH_PENALTY);
				LogManager::instance().CharLog(this, pkKiller->GetRaceNum(), "DEAD_BY_NPC", pkKiller->GetName());
			}
		}
		else
		{
			metin2::log::debug( "DEAD_BY_PC: {} {} KILLER {} {}", GetName(), static_cast<const void *>(this), pkKiller->GetName(), static_cast<const void *>(pkKiller));
			REMOVE_BIT(m_pointsInstant.instant_flag, INSTANT_FLAG_DEATH_PENALTY);

			if (GetEmpire() != pkKiller->GetEmpire())
			{
				int iEP = std::min(GetPoint(POINT_EMPIRE_POINT), pkKiller->GetPoint(POINT_EMPIRE_POINT));

				PointChange(POINT_EMPIRE_POINT, -(iEP / 10));
				pkKiller->PointChange(POINT_EMPIRE_POINT, iEP / 5);

				if (GetPoint(POINT_EMPIRE_POINT) < 10)
				{
					// TODO : need to add code that sends them to the entrance.
				}

				char buf[256];
				snprintf(buf, sizeof(buf),
						"%d %d %d %s %d %d %d %s",
						GetEmpire(), GetAlignment(), GetPKMode(), GetName(),
						pkKiller->GetEmpire(), pkKiller->GetAlignment(), pkKiller->GetPKMode(), pkKiller->GetName());

				LogManager::instance().CharLog(this, pkKiller->GetPlayerID(), "DEAD_BY_PC", buf);
			}
			else
			{
				if (!isAgreedPVP && !isUnderGuildWar && !IsKillerMode() && GetAlignment() >= 0 && !isDuel && !isForked)
				{
					int iNoPenaltyProb = 0;

					if (pkKiller->GetAlignment() >= 0)	// 1/3 percent down
						iNoPenaltyProb = 33;
					else				// 4/5 percent down
						iNoPenaltyProb = 20;

					if (number(1, 100) < iNoPenaltyProb)
						pkKiller->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("용신의 보호로 아이템이 떨어지지 않았습니다."));
					else
					{
						if (pkKiller->GetParty())
						{
							FPartyAlignmentCompute f(-20000, pkKiller->GetX(), pkKiller->GetY());
							pkKiller->GetParty()->ForEachOnlineMember(f);

							if (f.m_iCount == 0)
								pkKiller->UpdateAlignment(-20000);
							else
							{
								metin2::log::info( "ALIGNMENT PARTY count {:d} amount {:d}", f.m_iCount, f.m_iAmount);

								f.m_iStep = 1;
								pkKiller->GetParty()->ForEachOnlineMember(f);
							}
						}
						else
							pkKiller->UpdateAlignment(-20000);
					}
				}

				char buf[256];
				snprintf(buf, sizeof(buf),
						"%d %d %d %s %d %d %d %s",
						GetEmpire(), GetAlignment(), GetPKMode(), GetName(),
						pkKiller->GetEmpire(), pkKiller->GetAlignment(), pkKiller->GetPKMode(), pkKiller->GetName());

				LogManager::instance().CharLog(this, pkKiller->GetPlayerID(), "DEAD_BY_PC", buf);
			}
		}
	}
	else
	{
		metin2::log::debug( "DEAD: {} {}", GetName(), static_cast<const void *>(this));
		REMOVE_BIT(m_pointsInstant.instant_flag, INSTANT_FLAG_DEATH_PENALTY);
	}

	ClearSync();

	event_cancel(&m_pkStunEvent); // Kill the stun event.

	if (IsPC())
	{
		m_dwLastDeadTime = get_dword_time();
		SetKillerMode(false);
		GetDesc()->SetPhase(PHASE_DEAD);
	}
	else
	{
		// Monsters attacked by guards must give no reward.
		if (!IS_SET(m_pointsInstant.instant_flag, INSTANT_FLAG_NO_REWARD))
		{
			if (!(pkKiller && pkKiller->IsPC() && pkKiller->GetGuild() && pkKiller->GetGuild()->UnderAnyWar(GUILD_WAR_TYPE_FIELD)))
			{
				// Reviving monsters give no reward.
				if (GetMobTable().dwResurrectionVnum)
				{
					// DUNGEON_MONSTER_REBIRTH_BUG_FIX
					CHARACTER * chResurrect = CHARACTER_MANAGER::instance().SpawnMob(GetMobTable().dwResurrectionVnum, GetMapIndex(), GetX(), GetY(), GetZ(), true, (int) GetRotation());
					if (GetDungeon() && chResurrect)
					{
						chResurrect->SetDungeon(GetDungeon());
					}
					// END_OF_DUNGEON_MONSTER_REBIRTH_BUG_FIX

					Reward(false);
				}
				else if (IsRevive() == true)
				{
					Reward(false);
				}
				else
				{
					Reward(true); // Drops gold, item, etc..
				}
			}
			else
			{
				if (pkKiller->m_dwUnderGuildWarInfoMessageTime < get_dword_time())
				{
					pkKiller->m_dwUnderGuildWarInfoMessageTime = get_dword_time() + 60000;
					pkKiller->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("<길드> 길드전중에는 사냥에 따른 이익이 없습니다."));
				}
			}
		}
	}

	// BOSS_KILL_LOG
	if (GetMobRank() >= MOB_RANK_BOSS && pkKiller && pkKiller->IsPC())
	{
		char buf[51];
		snprintf(buf, sizeof(buf), "%d %d", metin2::config::server().channel, pkKiller->GetMapIndex());
		if (IsStone())
			LogManager::instance().CharLog(pkKiller, GetRaceNum(), "STONE_KILL", buf);
		else
			LogManager::instance().CharLog(pkKiller, GetRaceNum(), "BOSS_KILL", buf);
	}
	// END_OF_BOSS_KILL_LOG

	TPacketGCDead pack;
	pack.header	= HEADER_GC_DEAD;
	pack.vid	= m_vid;
	PacketAround(&pack, sizeof(pack));

	REMOVE_BIT(m_pointsInstant.instant_flag, INSTANT_FLAG_STUN);

	// If player character
	if (GetDesc() != nullptr) {
		//
		// Resend the effect packet to the client.
		//
		auto it = m_list_pkAffect.begin();

		while (it != m_list_pkAffect.end())
			SendAffectAddPacket(GetDesc(), *it++);
	}

	//
	// Create the Dead event,
	//
	// For monsters, the Dead event destroys them after a few seconds,
	// for PCs, it makes them come out in town after 3 minutes. Within 3 minutes the user
	// decides whether to restart in town or here.
	if (isDuel == false)
	{
		if (m_pkDeadEvent)
		{
			metin2::log::debug( "DEAD_EVENT_CANCEL: {} {} {}", GetName(), static_cast<const void *>(this), static_cast<const void *>(m_pkDeadEvent.get()));
			event_cancel(&m_pkDeadEvent);
		}

		if (IsStone())
			ClearStone();

		if (GetDungeon())
		{
			GetDungeon()->DeadCharacter(this);
		}

		SCharDeadEventInfo* pEventInfo = AllocEventInfo<SCharDeadEventInfo>();

		if (IsPC())
		{
			pEventInfo->isPC = true;
			pEventInfo->dwID = this->GetPlayerID();

			m_pkDeadEvent = event_create(dead_event, pEventInfo, PASSES_PER_SEC(180));
		}
		else
		{
			pEventInfo->isPC = false;
			pEventInfo->dwID = this->GetVID();

			if (IsRevive() == false && HasReviverInParty() == true)
			{
				m_pkDeadEvent = event_create(dead_event, pEventInfo, bImmediateDead ? 1 : PASSES_PER_SEC(3));
			}
			else
			{
				m_pkDeadEvent = event_create(dead_event, pEventInfo, bImmediateDead ? 1 : PASSES_PER_SEC(10));
			}
		}

		metin2::log::debug( "DEAD_EVENT_CREATE: {} {} {}", GetName(), static_cast<const void *>(this), static_cast<const void *>(m_pkDeadEvent.get()));
	}

	if (m_pkExchange != nullptr)
	{
		m_pkExchange->Cancel();
	}

	if (IsCubeOpen() == true)
	{
		Cube_close(this);
	}
	
	CShopManager::instance().StopShopping(this);
	CloseMyShop();
	CloseSafebox();

	if (true == IsMonster() && 2493 == GetMobTable().dwVnum)
	{
		if (nullptr != pkKiller && nullptr != pkKiller->GetGuild())
		{
			CDragonLairManager::instance().OnDragonDead( this, pkKiller->GetGuild()->GetID() );
		}
		else
		{
			metin2::log::error("DragonLair: Dragon killed by nobody");
		}
	}
}


void CHARACTER::SetLastAttacked(uint32_t dwTime)
{
	assert(m_pkMobInst != nullptr);

	m_pkMobInst->m_dwLastAttackedTime = dwTime;
	m_pkMobInst->m_posLastAttacked = GetXYZ();
}

void CHARACTER::SendDamagePacket(CHARACTER * pAttacker, int Damage, uint8_t DamageFlag)
{
	if (IsPC() == true || (pAttacker->IsPC() == true && pAttacker->GetTarget() == this))
	{
		TPacketGCDamageInfo damageInfo;
		memset(&damageInfo, 0, sizeof(TPacketGCDamageInfo));

		damageInfo.header = HEADER_GC_DAMAGE_INFO;
		damageInfo.dwVID = (uint32_t)GetVID();
		damageInfo.flag = DamageFlag;
		damageInfo.damage = Damage;

		if (GetDesc() != nullptr)
		{
			GetDesc()->Packet(&damageInfo, sizeof(TPacketGCDamageInfo));
		}

		if (pAttacker->GetDesc() != nullptr)
		{
			pAttacker->GetDesc()->Packet(&damageInfo, sizeof(TPacketGCDamageInfo));
		}
	}
}

//
// CHARACTER::Damage method makes this take damage.
//
// Arguments
//    pAttacker		: attacker
//    dam		: damage
//    EDamageType	: what kind of attack?
//    
// Return value
//    true		: dead
//    false		: not dead yet
// 
bool CHARACTER::Damage(CHARACTER * pAttacker, int dam, EDamageType type) // returns true if dead
{
	if (DAMAGE_TYPE_MAGIC == type)
	{
		dam = (int)((float)dam * (100 + (pAttacker->GetPoint(POINT_MAGIC_ATT_BONUS_PER) + pAttacker->GetPoint(POINT_MELEE_MAGIC_ATT_BONUS_PER))) / 100.f + 0.5f);
	}
	if (GetRaceNum() == 5001)
	{
		bool bDropMoney = false;
		int iPercent = (GetHP() * 100) / GetMaxHP();

		if (iPercent <= 10 && GetMaxSP() < 5)
		{
			SetMaxSP(5);
			bDropMoney = true;
		}
		else if (iPercent <= 20 && GetMaxSP() < 4)
		{
			SetMaxSP(4);
			bDropMoney = true;
		}
		else if (iPercent <= 40 && GetMaxSP() < 3)
		{
			SetMaxSP(3);
			bDropMoney = true;
		}
		else if (iPercent <= 60 && GetMaxSP() < 2)
		{
			SetMaxSP(2);
			bDropMoney = true;
		}
		else if (iPercent <= 80 && GetMaxSP() < 1)
		{
			SetMaxSP(1);
			bDropMoney = true;
		}

		if (bDropMoney)
		{
			uint32_t dwGold = 1000;
			int iSplitCount = number(10, 13);

			metin2::log::info( "WAEGU DropGoldOnHit {:d} times", GetMaxSP());

			for (int i = 1; i <= iSplitCount; ++i)
			{
				PIXEL_POSITION pos;
				CItem * item;

				if ((item = ITEM_MANAGER::instance().CreateItem(1, dwGold / iSplitCount)))
				{
					if (i != 0)
					{
						pos.x = (number(-14, 14) + number(-14, 14)) * 20;
						pos.y = (number(-14, 14) + number(-14, 14)) * 20;

						pos.x += GetX();
						pos.y += GetY();
					}

					item->AddToGround(GetMapIndex(), pos);
					item->StartDestroyEvent();
				}
			}
		}
	}

	// Handle fear when not a normal attack
	if (type != DAMAGE_TYPE_NORMAL && type != DAMAGE_TYPE_NORMAL_RANGE)
	{
		if (IsAffectFlag(AFF_TERROR))
		{
			int pct = GetSkillPower(SKILL_TERROR) / 400;

			if (number(1, 100) <= pct)
				return false;
		}
	}

	int iCurHP = GetHP();
	int iCurSP = GetSP();

	bool IsCritical = false;
	bool IsPenetrate = false;
	bool IsDeathBlow = false;

	enum DamageFlag
	{
		DAMAGE_NORMAL	= (1 << 0),
		DAMAGE_POISON	= (1 << 1),
		DAMAGE_DODGE	= (1 << 2),
		DAMAGE_BLOCK	= (1 << 3),
		DAMAGE_PENETRATE= (1 << 4),
		DAMAGE_CRITICAL = (1 << 5),
	};

	//
	// Magic and ranged skills (archer/assassin) get critical and penetration calculation.
	// Originally this shouldn't be done, but since a nerf (down-balance) patch wasn't possible, critical and
	// penetration are applied at /2 or more rather than their original values.
	// 
	// Melee skills added too, due to lots of complaints about warriors
	//
	// 20091109 : concluded that warriors ended up hugely overpowered; warrior ratio in Germany near 70%
	//
	if (type == DAMAGE_TYPE_MELEE || type == DAMAGE_TYPE_RANGE || type == DAMAGE_TYPE_MAGIC)
	{
		if (pAttacker)
		{
			// Critical
			int iCriticalPct = pAttacker->GetPoint(POINT_CRITICAL_PCT);

			if (!IsPC())
				iCriticalPct += pAttacker->GetMarriageBonus(UNIQUE_ITEM_MARRIAGE_CRITICAL_BONUS);

			if (iCriticalPct)
			{
				if (iCriticalPct >= 10) // Above 10: 5% + (1% per 4), so a value of 50 gives 20%
					iCriticalPct = 5 + (iCriticalPct - 10) / 4;
				else // Below 10: simply halved, 10 = 5%
					iCriticalPct /= 2;

				//Apply critical resistance.
				iCriticalPct -= GetPoint(POINT_RESIST_CRITICAL);

				if (number(1, 100) <= iCriticalPct)
				{
					IsCritical = true;
					dam *= 2;
					EffectPacket(SE_CRITICAL);

					if (IsAffectFlag(AFF_MANASHIELD))
					{
						RemoveAffect(AFF_MANASHIELD);
					}
				}
			}

			// Penetration
			int iPenetratePct = pAttacker->GetPoint(POINT_PENETRATE_PCT);

			if (!IsPC())
				iPenetratePct += pAttacker->GetMarriageBonus(UNIQUE_ITEM_MARRIAGE_PENETRATE_BONUS);


			if (iPenetratePct)
			{
				{
					CSkillProto* pkSk = CSkillManager::instance().Get(SKILL_RESIST_PENETRATE);

					if (nullptr != pkSk)
					{
						pkSk->SetPointVar("k", 1.0f * GetSkillPower(SKILL_RESIST_PENETRATE) / 100.0f);

						iPenetratePct -= static_cast<int>(pkSk->kPointPoly.Eval());
					}
				}

				if (iPenetratePct >= 10)
				{
					// Above 10: 5% + (1% per 4), so a value of 50 gives 20%
					iPenetratePct = 5 + (iPenetratePct - 10) / 4;
				}
				else
				{
					// Below 10: simply halved, 10 = 5%
					iPenetratePct /= 2;
				}

				//Apply penetration resistance.
				iPenetratePct -= GetPoint(POINT_RESIST_PENETRATE);

				if (number(1, 100) <= iPenetratePct)
				{
					IsPenetrate = true;

					if (metin2::config::test_server())
						ChatPacket(CHAT_TYPE_INFO, LC_TEXT("관통 추가 데미지 %d"), GetPoint(POINT_DEF_GRADE) * (100 + GetPoint(POINT_DEF_BONUS)) / 100);

					dam += GetPoint(POINT_DEF_GRADE) * (100 + GetPoint(POINT_DEF_BONUS)) / 100;

					if (IsAffectFlag(AFF_MANASHIELD))
					{
						RemoveAffect(AFF_MANASHIELD);
					}
				}
			}
		}
	}
	// 
	// Attribute values are calculated only for combo and bow attacks, i.e. normal attacks.
	//
	else if (type == DAMAGE_TYPE_NORMAL || type == DAMAGE_TYPE_NORMAL_RANGE)
	{
		if (type == DAMAGE_TYPE_NORMAL)
		{
			// Melee normal attacks can be blocked
			if (GetPoint(POINT_BLOCK) && number(1, 100) <= GetPoint(POINT_BLOCK))
			{
				if (metin2::config::test_server())
				{
					pAttacker->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%s 블럭! (%d%%)"), GetName(), GetPoint(POINT_BLOCK));
					ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%s 블럭! (%d%%)"), GetName(), GetPoint(POINT_BLOCK));
				}

				SendDamagePacket(pAttacker, 0, DAMAGE_BLOCK);
				return false;
			}
		}
		else if (type == DAMAGE_TYPE_NORMAL_RANGE)
		{
			// Ranged normal attacks can be dodged
			if (GetPoint(POINT_DODGE) && number(1, 100) <= GetPoint(POINT_DODGE))
			{
				if (metin2::config::test_server())
				{
					pAttacker->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%s 회피! (%d%%)"), GetName(), GetPoint(POINT_DODGE));
					ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%s 회피! (%d%%)"), GetName(), GetPoint(POINT_DODGE));
				}

				SendDamagePacket(pAttacker, 0, DAMAGE_DODGE);
				return false;
			}
		}

		if (IsAffectFlag(AFF_JEONGWIHON))
			dam = (int) (dam * (100 + GetSkillPower(SKILL_JEONGWI) * 25 / 100) / 100);

		if (IsAffectFlag(AFF_TERROR))
			dam = (int) (dam * (95 - GetSkillPower(SKILL_TERROR) / 5) / 100);

		if (IsAffectFlag(AFF_HOSIN))
			dam = dam * (100 - GetPoint(POINT_RESIST_NORMAL_DAMAGE)) / 100;

		//
		// Apply attacker attributes
		//
		if (pAttacker)
		{
			if (type == DAMAGE_TYPE_NORMAL)
			{
				// Reflect
				if (GetPoint(POINT_REFLECT_MELEE))
				{
					int reflectDamage = dam * GetPoint(POINT_REFLECT_MELEE) / 100;

					// NOTE: design requested that if the attacker has IMMUNE_REFLECT, instead of not reflecting 
					// at all, the damage is fixed at 1/3.
					if (pAttacker->IsImmune(IMMUNE_REFLECT))
						reflectDamage = int(reflectDamage / 3.0f + 0.5f);

					pAttacker->Damage(this, reflectDamage, DAMAGE_TYPE_SPECIAL);
				}
			}

			// Critical
			int iCriticalPct = pAttacker->GetPoint(POINT_CRITICAL_PCT);

			if (!IsPC())
				iCriticalPct += pAttacker->GetMarriageBonus(UNIQUE_ITEM_MARRIAGE_CRITICAL_BONUS);

			if (iCriticalPct)
			{
				//Apply critical resistance.
				iCriticalPct -= GetPoint(POINT_RESIST_CRITICAL);

				if (number(1, 100) <= iCriticalPct)
				{
					IsCritical = true;
					dam *= 2;
					EffectPacket(SE_CRITICAL);
				}
			}

			// Penetration
			int iPenetratePct = pAttacker->GetPoint(POINT_PENETRATE_PCT);

			if (!IsPC())
				iPenetratePct += pAttacker->GetMarriageBonus(UNIQUE_ITEM_MARRIAGE_PENETRATE_BONUS);

			{
				CSkillProto* pkSk = CSkillManager::instance().Get(SKILL_RESIST_PENETRATE);

				if (nullptr != pkSk)
				{
					pkSk->SetPointVar("k", 1.0f * GetSkillPower(SKILL_RESIST_PENETRATE) / 100.0f);

					iPenetratePct -= static_cast<int>(pkSk->kPointPoly.Eval());
				}
			}


			if (iPenetratePct)
			{
				
				//Apply penetration resistance.
				iPenetratePct -= GetPoint(POINT_RESIST_PENETRATE);

				if (number(1, 100) <= iPenetratePct)
				{
					IsPenetrate = true;

					if (metin2::config::test_server())
						ChatPacket(CHAT_TYPE_INFO, LC_TEXT("관통 추가 데미지 %d"), GetPoint(POINT_DEF_GRADE) * (100 + GetPoint(POINT_DEF_BONUS)) / 100);
					dam += GetPoint(POINT_DEF_GRADE) * (100 + GetPoint(POINT_DEF_BONUS)) / 100;
				}
			}

			// HP steal
			if (pAttacker->GetPoint(POINT_STEAL_HP))
			{
				int pct = 1;

				if (number(1, 10) <= pct)
				{
					int iHP = std::min(dam, std::max(0, iCurHP)) * pAttacker->GetPoint(POINT_STEAL_HP) / 100;

					if (iHP > 0 && GetHP() >= iHP)
					{
						CreateFly(FLY_HP_SMALL, pAttacker);
						pAttacker->PointChange(POINT_HP, iHP);
						PointChange(POINT_HP, -iHP);
					}
				}
			}

			// SP steal
			if (pAttacker->GetPoint(POINT_STEAL_SP))
			{
				int pct = 1;

				if (number(1, 10) <= pct)
				{
					int iCur;

					if (IsPC())
						iCur = iCurSP;
					else
						iCur = iCurHP;

					int iSP = std::min(dam, std::max(0, iCur)) * pAttacker->GetPoint(POINT_STEAL_SP) / 100;

					if (iSP > 0 && iCur >= iSP)
					{
						CreateFly(FLY_SP_SMALL, pAttacker);
						pAttacker->PointChange(POINT_SP, iSP);

						if (IsPC())
							PointChange(POINT_SP, -iSP);
					}
				}
			}

			// Gold steal
			if (pAttacker->GetPoint(POINT_STEAL_GOLD))
			{
				if (number(1, 100) <= pAttacker->GetPoint(POINT_STEAL_GOLD))
				{
					int iAmount = number(1, GetLevel());
					pAttacker->PointChange(POINT_GOLD, iAmount);
					DBManager::instance().SendMoneyLog(MONEY_LOG_MISC, 1, iAmount);
				}
			}

			// HP recovery on every hit
			if (pAttacker->GetPoint(POINT_HIT_HP_RECOVERY) && number(0, 4) > 0) // 80% chance
			{
				int i = ((iCurHP>=0)?std::min(dam, iCurHP):dam) * pAttacker->GetPoint(POINT_HIT_HP_RECOVERY) / 100; // Fix

				if (i)
				{
					CreateFly(FLY_HP_SMALL, pAttacker);
					pAttacker->PointChange(POINT_HP, i);
				}
			}

			// SP recovery on every hit
			if (pAttacker->GetPoint(POINT_HIT_SP_RECOVERY) && number(0, 4) > 0) // 80% chance
			{
				int i = ((iCurHP>=0)?std::min(dam, iCurHP):dam) * pAttacker->GetPoint(POINT_HIT_SP_RECOVERY) / 100; // Fix

				if (i)
				{
					CreateFly(FLY_SP_SMALL, pAttacker);
					pAttacker->PointChange(POINT_SP, i);
				}
			}

			// Remove the target's mana.
			if (pAttacker->GetPoint(POINT_MANA_BURN_PCT))
			{
				if (number(1, 100) <= pAttacker->GetPoint(POINT_MANA_BURN_PCT))
					PointChange(POINT_SP, -50);
			}
		}
	}

	//
	// Bonus damage/defense from normal attacks or skills
	// 
	switch (type)
	{
		case DAMAGE_TYPE_NORMAL:
		case DAMAGE_TYPE_NORMAL_RANGE:
			if (pAttacker)
				if (pAttacker->GetPoint(POINT_NORMAL_HIT_DAMAGE_BONUS))
					dam = dam * (100 + pAttacker->GetPoint(POINT_NORMAL_HIT_DAMAGE_BONUS)) / 100;

			dam = dam * (100 - std::min(99, GetPoint(POINT_NORMAL_HIT_DEFEND_BONUS))) / 100;
			break;

		case DAMAGE_TYPE_MELEE:
		case DAMAGE_TYPE_RANGE:
		case DAMAGE_TYPE_FIRE:
		case DAMAGE_TYPE_ICE:
		case DAMAGE_TYPE_ELEC:
		case DAMAGE_TYPE_MAGIC:
			if (pAttacker)
				if (pAttacker->GetPoint(POINT_SKILL_DAMAGE_BONUS))
					dam = dam * (100 + pAttacker->GetPoint(POINT_SKILL_DAMAGE_BONUS)) / 100;

			dam = dam * (100 - std::min(99, GetPoint(POINT_SKILL_DEFEND_BONUS))) / 100;
			break;

		default:
			break;
	}

	//
	// Mana shield (Dark Protection)
	//
	if (IsAffectFlag(AFF_MANASHIELD))
	{
		// The smaller POINT_MANASHIELD is, the better
		int iDamageSPPart = dam / 3;
		int iDamageToSP = iDamageSPPart * GetPoint(POINT_MANASHIELD) / 100;
		int iSP = GetSP();

		// If there is SP, damage is always halved
		if (iDamageToSP <= iSP)
		{
			PointChange(POINT_SP, -iDamageToSP);
			dam -= iDamageSPPart;
		}
		else
		{
			// When there isn't enough SP and more HP must be taken
			PointChange(POINT_SP, -GetSP());
			dam -= iSP * 100 / std::max(GetPoint(POINT_MANASHIELD), 1);
		}
	}

	//
	// Overall defense increase (mall item)
	// 
	if (GetPoint(POINT_MALL_DEFBONUS) > 0)
	{
		int dec_dam = std::min(200, dam * GetPoint(POINT_MALL_DEFBONUS) / 100);
		dam -= dec_dam;
	}

	if (pAttacker)
	{
		//
		// Overall attack increase (mall item)
		//
		if (pAttacker->GetPoint(POINT_MALL_ATTBONUS) > 0)
		{
			int add_dam = std::min(300, dam * pAttacker->GetLimitPoint(POINT_MALL_ATTBONUS) / 100);
			dam += add_dam;
		}

		//
		// Empire bonus (old Korean version only)
		//
		int iEmpire = GetEmpire();
		int32_t lMapIndex = GetMapIndex();
		int iMapEmpire = SECTREE_MANAGER::instance().GetEmpireFromMapIndex(lMapIndex);


		if (pAttacker->IsPC())
		{
			iEmpire = pAttacker->GetEmpire();
			lMapIndex = pAttacker->GetMapIndex();
			iMapEmpire = SECTREE_MANAGER::instance().GetEmpireFromMapIndex(lMapIndex);

			// 10% less damage against people of another empire
			if (iEmpire && iMapEmpire && iEmpire != iMapEmpire)
			{
				int percent = 10;
				
				if (184 <= lMapIndex && lMapIndex <= 189)
				{
					percent = 9;
				}
				else
				{
					percent = 9;
				}

				dam = dam * percent / 10;
			}

			if (!IsPC() && GetMonsterDrainSPPoint())
			{
				int iDrain = GetMonsterDrainSPPoint();

				if (iDrain <= pAttacker->GetSP())
					pAttacker->PointChange(POINT_SP, -iDrain);
				else
				{
					int iSP = pAttacker->GetSP();
					pAttacker->PointChange(POINT_SP, -iSP);
				}
			}

		}
		else if (pAttacker->IsGuardNPC())
		{
			SET_BIT(m_pointsInstant.instant_flag, INSTANT_FLAG_NO_REWARD);
			Stun();
			return true;
		}

		//
		// Monarch's Diamond Fist & Lion's Roar 
		//
		if (pAttacker->IsPC() && CMonarch::instance().IsPowerUp(pAttacker->GetEmpire()))
		{
			// 10% more damage
			dam += dam / 10;
		}

		if (IsPC() && CMonarch::instance().IsDefenceUp(GetEmpire()))
		{
			// 10% less damage
			dam -= dam / 10;
		}
	}

	if (!GetSectree() || GetSectree()->IsAttr(GetX(), GetY(), ATTR_BANPK))
		return false;

	if (!IsPC())
	{
		if (m_pkParty && m_pkParty->GetLeader())
			m_pkParty->GetLeader()->SetLastAttacked(get_dword_time());
		else
			SetLastAttacked(get_dword_time());

		// Monster dialogue: when hit
		MonsterChat(MONSTER_CHAT_ATTACKED);
	}

	if (IsStun())
	{
		Dead(pAttacker);
		return true;
	}

	if (IsDead())
		return true;

	// Prevent dying from poison attacks.
	if (type == DAMAGE_TYPE_POISON)
	{
		if (GetHP() - dam <= 0)
		{
			dam = GetHP() - 1;
		}
	}

	// ------------------------
	// German premium mode 
	// -----------------------

	// STONE SKIN : damage halved
	if (IsMonster() && IsStoneSkinner())
	{
		if (GetHPPct() < GetMobTable().bStoneSkinPoint)
			dam /= 2;
	}

	if (pAttacker)
	{
		// DEATH BLOW : chance of 4x damage (!? currently only used by event or siege monsters)
		if (pAttacker->IsMonster() && pAttacker->IsDeathBlower())
		{
			if (pAttacker->IsDeathBlow())
			{
				//if (number(1, 4) == GetJob())
				if (number(JOB_WARRIOR, JOB_MAX_NUM - 1) == GetJob())
				{
					IsDeathBlow = true;
					dam = dam * 4;
				}
			}
		}

		dam = BlueDragon_Damage(this, pAttacker, dam);

		uint8_t damageFlag = 0;

		if (type == DAMAGE_TYPE_POISON)
			damageFlag = DAMAGE_POISON;
		else
			damageFlag = DAMAGE_NORMAL;

		if (IsCritical == true)
			damageFlag |= DAMAGE_CRITICAL;

		if (IsPenetrate == true)
			damageFlag |= DAMAGE_PENETRATE;


		//Final damage adjustment
		float damMul = this->GetDamMul();
		float tempDam = dam;
		dam = tempDam * damMul + 0.5f;


		if (pAttacker)
			SendDamagePacket(pAttacker, dam, damageFlag);

		if (metin2::config::test_server())
		{
			if(pAttacker)
			{
				pAttacker->ChatPacket(CHAT_TYPE_INFO, "-> %s, DAM %d HP %d(%d%%) %s%s",
						GetName(), 
						dam, 
						GetHP(),
						(GetHP() * 100) / GetMaxHP(),
						IsCritical ? "crit " : "",
						IsPenetrate ? "pene " : "",
						IsDeathBlow ? "deathblow " : "");
			}

			ChatPacket(CHAT_TYPE_PARTY, "<- %s, DAM %d HP %d(%d%%) %s%s",
					pAttacker ? pAttacker->GetName() : nullptr,
					dam, 
					GetHP(),
					(GetHP() * 100) / GetMaxHP(),
					IsCritical ? "crit " : "",
					IsPenetrate ? "pene " : "",
					IsDeathBlow ? "deathblow " : "");
		}

		if (m_bDetailLog)
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%s[%d]가 공격 위치: %d %d"), pAttacker->GetName(), (uint32_t) pAttacker->GetVID(), pAttacker->GetX(), pAttacker->GetY());
		}
	}

	//
	// !!!!!!!!! Part that actually reduces HP !!!!!!!!!
	//
	if (!cannot_dead)
	{
		PointChange(POINT_HP, -dam, false);
	}

	if (pAttacker && dam > 0 && IsNPC())
	{
		TDamageMap::iterator it = m_map_kDamage.find(pAttacker->GetVID());

		if (it == m_map_kDamage.end())
		{
			m_map_kDamage.insert(TDamageMap::value_type(pAttacker->GetVID(), TBattleInfo(dam, 0)));
			it = m_map_kDamage.find(pAttacker->GetVID());
		}
		else
		{
			it->second.iTotalDamage += dam;
		}

		StartRecoveryEvent(); // Monsters start recovering when they take damage.

		UpdateAggrPointEx(pAttacker, type, dam, it->second);
	}

	if (GetHP() <= 0)
	{
		Stun();

		if (pAttacker && !pAttacker->IsNPC())
			m_dwKillerPID = pAttacker->GetPlayerID();
		else
			m_dwKillerPID = 0;
	}

	return false;
}

void CHARACTER::DistributeHP(CHARACTER * pkKiller)
{
	if (pkKiller->GetDungeon()) // Dumplings don't appear inside dungeons
		return;
}

static void GiveExp(CHARACTER * from, CHARACTER * to, int iExp)
{
	// Experience adjustment ratio by level difference
	iExp = CALCULATE_VALUE_LVDELTA(to->GetLevel(), from->GetLevel(), iExp);

	// 3x experience bonus on external test server
	if (metin2::config::game().distribution_test_server)
		iExp *= 3;

	int iBaseExp = iExp;

	// Apply fortune-telling and company experience events
	iExp = iExp * (100 + CPrivManager::instance().GetPriv(to, PRIV_EXP_PCT)) / 100;

	// Default in-game experience bonus
	{
		// Labor Day medal
		if (to->IsEquipUniqueItem(UNIQUE_ITEM_LARBOR_MEDAL))
			iExp += iExp * 20 /100;

		// Demon Tower experience bonus
		if (to->GetMapIndex() >= 660000 && to->GetMapIndex() < 670000) 
			iExp += iExp * 20 / 100; // 1.2x (20%)

		// Item double-experience attribute
		if (to->GetPoint(POINT_EXP_DOUBLE_BONUS))
			if (number(1, 100) <= to->GetPoint(POINT_EXP_DOUBLE_BONUS))
				iExp += iExp * 30 / 100; // 1.3x (30%)

		// Ring of Experience (2-hour)
		if (to->IsEquipUniqueItem(UNIQUE_ITEM_DOUBLE_EXP))
			iExp += iExp * 50 / 100;

		switch (to->GetMountVnum())
		{
			case 20110:
			case 20111:
			case 20112:
			case 20113:
				if (to->IsEquipUniqueItem(71115) || to->IsEquipUniqueItem(71117) || to->IsEquipUniqueItem(71119) ||
						to->IsEquipUniqueItem(71121) )
				{
					iExp += iExp * 10 / 100;
				}
				break;

			case 20114:
			case 20120:
			case 20121:
			case 20122:
			case 20123:
			case 20124:
			case 20125:
				// White Lion experience bonus
				iExp += iExp * 30 / 100;
				break;
		}
	}

	// Item mall experience bonus
	// Item mall: experience purchase
	if (to->GetPremiumRemainSeconds(PREMIUM_EXP) > 0)
	{
		iExp += (iExp * 50 / 100);
	}

	if (to->IsEquipUniqueGroup(UNIQUE_GROUP_RING_OF_EXP) == true)
	{
		iExp += (iExp * 50 / 100);
	}

	// Marriage bonus
	iExp += iExp * to->GetMarriageBonus(UNIQUE_ITEM_MARRIAGE_EXP_BONUS) / 100;

	iExp += (iExp * to->GetPoint(POINT_RAMADAN_CANDY_BONUS_EXP)/100);
	iExp += (iExp * to->GetPoint(POINT_MALL_EXPBONUS)/100);
	iExp += (iExp * to->GetPoint(POINT_EXP)/100);

	if (metin2::config::test_server())
	{
		metin2::log::info( "Bonus Exp : Ramadan Candy: {:d} MallExp: {:d} PointExp: {:d}", 
				to->GetPoint(POINT_RAMADAN_CANDY_BONUS_EXP),
				to->GetPoint(POINT_MALL_EXPBONUS),
				to->GetPoint(POINT_EXP)
			   );
	}

	// Design-side adjustment value, 85% as of 2005.04.21
	iExp = iExp * CHARACTER_MANAGER::instance().GetMobExpRate(to) / 100;

	// Limit on experience gained at once
	iExp = std::min<int>(to->GetNextExp() / 10, iExp);

	if (metin2::config::test_server())
	{
		if (quest::CQuestManager::instance().GetEventFlag("exp_bonus_log") && iBaseExp>0)
			to->ChatPacket(CHAT_TYPE_INFO, "exp bonus %d%%", (iExp-iBaseExp)*100/iBaseExp);
	}

	iExp = AdjustExpByLevel(to, iExp);

	to->PointChange(POINT_EXP, iExp, true);
	from->CreateFly(FLY_EXP, to);

	{
		CHARACTER * you = to->GetMarryPartner();
		// Love points rise when a married couple are in a party together
		if (you)
		{
			// 100 million is 100%
			uint32_t dwUpdatePoint = 2000*iExp/to->GetLevel()/to->GetLevel()/3;

			if (to->GetPremiumRemainSeconds(PREMIUM_MARRIAGE_FAST) > 0 || 
					you->GetPremiumRemainSeconds(PREMIUM_MARRIAGE_FAST) > 0)
				dwUpdatePoint = (uint32_t)(dwUpdatePoint * 3);

			marriage::TMarriage* pMarriage = marriage::CManager::instance().Get(to->GetPlayerID());

			// DIVORCE_NULL_BUG_FIX
			if (pMarriage && pMarriage->IsNear())
				pMarriage->Update(dwUpdatePoint);
			// END_OF_DIVORCE_NULL_BUG_FIX
		}
	}
}

namespace NPartyExpDistribute
{
	struct FPartyTotaler
	{
		int		total;
		int		member_count;
		int		x, y;

		FPartyTotaler(CHARACTER * center)
			: total(0), member_count(0), x(center->GetX()), y(center->GetY())
		{};

		void operator () (CHARACTER * ch)
		{
			if (DISTANCE_APPROX(ch->GetX() - x, ch->GetY() - y) <= PARTY_DEFAULT_RANGE)
			{
				total += party_exp_distribute_table[ch->GetLevel()];

				++member_count;
			}
		}
	};

	struct FPartyDistributor
	{
		int		total;
		CHARACTER *	c;
		int		x, y;
		uint32_t		_iExp;
		int		m_iMode;
		int		m_iMemberCount;

		FPartyDistributor(CHARACTER * center, int member_count, int total, uint32_t iExp, int iMode) 
			: total(total), c(center), x(center->GetX()), y(center->GetY()), _iExp(iExp), m_iMode(iMode), m_iMemberCount(member_count)
			{
				if (m_iMemberCount == 0)
					m_iMemberCount = 1;
			};

		void operator () (CHARACTER * ch)
		{
			if (DISTANCE_APPROX(ch->GetX() - x, ch->GetY() - y) <= PARTY_DEFAULT_RANGE)
			{
				uint32_t iExp2 = 0;

				switch (m_iMode)
				{
					case PARTY_EXP_DISTRIBUTION_NON_PARITY:
						iExp2 = (uint32_t) (_iExp * (float) party_exp_distribute_table[ch->GetLevel()] / total);
						break;

					case PARTY_EXP_DISTRIBUTION_PARITY:
						iExp2 = _iExp / m_iMemberCount;
						break;

					default:
						metin2::log::error("Unknown party exp distribution mode {:d}", m_iMode);
						return;
				}

				GiveExp(c, ch, iExp2);
			}
		}
	};
}

struct TDamageInfo {
	int iDam;
	CHARACTER * pAttacker;
	CParty * pParty;

	void Clear()
	{
		pAttacker = nullptr;
		pParty = nullptr;
	}

	inline void Distribute(CHARACTER * ch, int iExp)
	{
		if (pAttacker)
			GiveExp(ch, pAttacker, iExp);
		else if (pParty)
		{
			NPartyExpDistribute::FPartyTotaler f(ch);
			pParty->ForEachOnlineMember(f);

			if (pParty->IsPositionNearLeader(ch))
				iExp = iExp * (100 + pParty->GetExpBonusPercent()) / 100;

			if (metin2::config::test_server())
			{
				if (quest::CQuestManager::instance().GetEventFlag("exp_bonus_log") && pParty->GetExpBonusPercent())
					pParty->ChatPacketToAllMember(CHAT_TYPE_INFO, "exp party bonus %d%%", pParty->GetExpBonusPercent());
			}

			// Experience funneling (5% of party experience is taken and given first)
			if (pParty->GetExpCentralizeCharacter())
			{
				CHARACTER * tch = pParty->GetExpCentralizeCharacter();

				if (DISTANCE_APPROX(ch->GetX() - tch->GetX(), ch->GetY() - tch->GetY()) <= PARTY_DEFAULT_RANGE)
				{
					int iExpCenteralize = (int) (iExp * 0.05f);
					iExp -= iExpCenteralize;

					GiveExp(ch, pParty->GetExpCentralizeCharacter(), iExpCenteralize);
				}
			}

			NPartyExpDistribute::FPartyDistributor fDist(ch, f.member_count, f.total, iExp, pParty->GetExpDistributionMode());
			pParty->ForEachOnlineMember(fDist);
		}
	}
};

CHARACTER * CHARACTER::DistributeExp()
{
	int iExpToDistribute = GetExp();

	if (iExpToDistribute <= 0)
		return nullptr;

	int	iTotalDam = 0;
	CHARACTER * pkChrMostAttacked = nullptr;
	int iMostDam = 0;

	typedef std::vector<TDamageInfo> TDamageInfoTable;
	TDamageInfoTable damage_info_table;
	std::map<CParty *, TDamageInfo> map_party_damage;

	damage_info_table.reserve(m_map_kDamage.size());

	TDamageMap::iterator it = m_map_kDamage.begin();

	// First filter out those not nearby. (50m)
	while (it != m_map_kDamage.end())
	{
		const VID & c_VID = it->first;
		int iDam = it->second.iTotalDamage;

		++it;

		CHARACTER * pAttacker = CHARACTER_MANAGER::instance().Find(c_VID);

		// Do NPCs even hit? -.-;
		if (!pAttacker || pAttacker->IsNPC() || DISTANCE_APPROX(GetX()-pAttacker->GetX(), GetY()-pAttacker->GetY())>5000)
			continue;

		iTotalDam += iDam;
		if (!pkChrMostAttacked || iDam > iMostDam)
		{
			pkChrMostAttacked = pAttacker;
			iMostDam = iDam;
		}

		if (pAttacker->GetParty())
		{
			std::map<CParty *, TDamageInfo>::iterator it = map_party_damage.find(pAttacker->GetParty());
			if (it == map_party_damage.end())
			{
				TDamageInfo di;
				di.iDam = iDam;
				di.pAttacker = nullptr;
				di.pParty = pAttacker->GetParty();
				map_party_damage.insert(std::make_pair(di.pParty, di));
			}
			else
			{
				it->second.iDam += iDam;
			}
		}
		else
		{
			TDamageInfo di;

			di.iDam = iDam;
			di.pAttacker = pAttacker;
			di.pParty = nullptr;

			damage_info_table.push_back(di);
		}
	}

	for (std::map<CParty *, TDamageInfo>::iterator it = map_party_damage.begin(); it != map_party_damage.end(); ++it)
	{
		damage_info_table.push_back(it->second);
	}

	SetExp(0);

	if (iTotalDam == 0)	// Return if damage dealt is 0
		return nullptr;

	if (m_pkChrStone)	// If there is a stone, give half of the experience to the stone.
	{
		int iExp = iExpToDistribute >> 1;
		m_pkChrStone->SetExp(m_pkChrStone->GetExp() + iExp);
		iExpToDistribute -= iExp;
	}

	metin2::log::debug( "{} total exp: {:d}, damage_info_table.size() == {:d}, TotalDam {:d}",
			GetName(), iExpToDistribute, damage_info_table.size(), iTotalDam);

	if (damage_info_table.empty())
		return nullptr;

	// The one who dealt the most damage recovers HP.
	DistributeHP(pkChrMostAttacked);	// Dumpling system

	{
		// The person or party that dealt the most damage gets 20% of total experience + experience proportional to their damage.
		TDamageInfoTable::iterator di = damage_info_table.begin();
		{
			TDamageInfoTable::iterator it;

			for (it = damage_info_table.begin(); it != damage_info_table.end();++it)
			{
				if (it->iDam > di->iDam)
					di = it;
			}
		}

		int	iExp = iExpToDistribute / 5;
		iExpToDistribute -= iExp;

		float fPercent = (float) di->iDam / iTotalDam;

		if (fPercent > 1.0f)
		{
			metin2::log::error("DistributeExp percent over 1.0 (fPercent {:f} name {})", fPercent, di->pAttacker->GetName());
			fPercent = 1.0f;
		}

		iExp += (int) (iExpToDistribute * fPercent);

		di->Distribute(this, iExp);

		// Return if all 100% has been taken.
		if (fPercent == 1.0f)
			return pkChrMostAttacked;

		di->Clear();
	}

	{
		// Distribute the remaining 80% of experience.
		TDamageInfoTable::iterator it;

		for (it = damage_info_table.begin(); it != damage_info_table.end(); ++it)
		{
			TDamageInfo & di = *it;

			float fPercent = (float) di.iDam / iTotalDam;

			if (fPercent > 1.0f)
			{
				metin2::log::error("DistributeExp percent over 1.0 (fPercent {:f} name {})", fPercent, di.pAttacker->GetName());
				fPercent = 1.0f;
			}

			di.Distribute(this, (int) (iExpToDistribute * fPercent));
		}
	}

	return pkChrMostAttacked;
}

// Returns the arrow count
int CHARACTER::GetArrowAndBow(CItem * * ppkBow, CItem * * ppkArrow, int iArrowCount/* = 1 */)
{
	CItem * pkBow;

	if (!(pkBow = GetWear(WEAR_WEAPON)) || pkBow->GetProto()->bSubType != WEAPON_BOW)
	{
		return 0;
	}

	CItem * pkArrow;

	if (!(pkArrow = GetWear(WEAR_ARROW)) || pkArrow->GetType() != ITEM_WEAPON ||
			pkArrow->GetProto()->bSubType != WEAPON_ARROW)
	{
		return 0;
	}

	iArrowCount = std::min<int>(iArrowCount, pkArrow->GetCount());

	*ppkBow = pkBow;
	*ppkArrow = pkArrow;

	return iArrowCount;
}

void CHARACTER::UseArrow(CItem * pkArrow, uint32_t dwArrowCount)
{
	int iCount = pkArrow->GetCount();
	uint32_t dwVnum = pkArrow->GetVnum();
	iCount = iCount - std::min<int>(iCount, dwArrowCount);
	pkArrow->SetCount(iCount);

	if (iCount == 0)
	{
		CItem * pkNewArrow = FindSpecifyItem(dwVnum);

		metin2::log::info( "UseArrow : FindSpecifyItem {:d} {}", dwVnum, static_cast<const void *>(pkNewArrow));

		if (pkNewArrow)
			EquipItem(pkNewArrow);
	}
}

class CFuncShoot
{
	public:
		CHARACTER *	m_me;
		uint8_t		m_bType;
		bool		m_bSucceed;

		CFuncShoot(CHARACTER * ch, uint8_t bType) : m_me(ch), m_bType(bType), m_bSucceed(false)
		{
		}

		void operator () (uint32_t dwTargetVID)
		{
			if (m_bType > 1)
			{
				if (metin2::config::game().skill_disable)
					return;

				m_me->m_SkillUseInfo[m_bType].SetMainTargetVID(dwTargetVID);
			}

			CHARACTER * pkVictim = CHARACTER_MANAGER::instance().Find(dwTargetVID);

			if (!pkVictim)
				return;

			// Cannot attack
			if (!battle_is_attackable(m_me, pkVictim))
				return;

			if (m_me->IsNPC())
			{
				if (DISTANCE_APPROX(m_me->GetX() - pkVictim->GetX(), m_me->GetY() - pkVictim->GetY()) > 5000)
					return;
			}

			CItem * pkBow;
			CItem * pkArrow;

			switch (m_bType)
			{
				case 0: // Normal bow
					{
						int iDam = 0;

						if (m_me->IsPC())
						{
							if (m_me->GetJob() != JOB_ASSASSIN)
								return;

							if (0 == m_me->GetArrowAndBow(&pkBow, &pkArrow))
								return;

							if (m_me->GetSkillGroup() != 0)
								if (!m_me->IsNPC() && m_me->GetSkillGroup() != 2)
								{
									if (m_me->GetSP() < 5)
										return;

									m_me->PointChange(POINT_SP, -5);
								}

							iDam = CalcArrowDamage(m_me, pkVictim, pkBow, pkArrow);
							m_me->UseArrow(pkArrow, 1);

							// check speed hack
							uint32_t	dwCurrentTime	= get_dword_time();
							if (IS_SPEED_HACK(m_me, pkVictim, dwCurrentTime))
								iDam	= 0;
						}
						else
							iDam = CalcMeleeDamage(m_me, pkVictim);

						NormalAttackAffect(m_me, pkVictim);

						// Damage calculation
						iDam = iDam * (100 - pkVictim->GetPoint(POINT_RESIST_BOW)) / 100;

						m_me->OnMove(true);
						pkVictim->OnMove();

						if (pkVictim->CanBeginFight())
							pkVictim->BeginFight(m_me);

						pkVictim->Damage(m_me, iDam, DAMAGE_TYPE_NORMAL_RANGE);
						// End of hit damage calculation
					}
					break;

				case 1: // Normal magic
					{
						int iDam;

						if (m_me->IsPC())
							return;

						iDam = CalcMagicDamage(m_me, pkVictim);

						NormalAttackAffect(m_me, pkVictim);

						// Damage calculation
						iDam = iDam * (100 - pkVictim->GetPoint(POINT_RESIST_MAGIC)) / 100;

						m_me->OnMove(true);
						pkVictim->OnMove();

						if (pkVictim->CanBeginFight())
							pkVictim->BeginFight(m_me); 

						pkVictim->Damage(m_me, iDam, DAMAGE_TYPE_MAGIC);
						// End of hit damage calculation
					}
					break;

				case SKILL_YEONSA:	// Rapid fire
					{
						int iUseArrow = 1;

						// When only computing the total
						{
							if (iUseArrow == m_me->GetArrowAndBow(&pkBow, &pkArrow, iUseArrow))
							{
								m_me->OnMove(true);
								pkVictim->OnMove();

								if (pkVictim->CanBeginFight())
									pkVictim->BeginFight(m_me); 

								m_me->ComputeSkill(m_bType, pkVictim);
								m_me->UseArrow(pkArrow, iUseArrow);

								if (pkVictim->IsDead())
									break;

							}
							else
								break;
						}
					}
					break;


				case SKILL_KWANKYEOK:
					{
						int iUseArrow = 1;

						if (iUseArrow == m_me->GetArrowAndBow(&pkBow, &pkArrow, iUseArrow))
						{
							m_me->OnMove(true);
							pkVictim->OnMove();

							if (pkVictim->CanBeginFight())
								pkVictim->BeginFight(m_me); 

							metin2::log::info( "{} kwankeyok {}", m_me->GetName(), pkVictim->GetName());
							m_me->ComputeSkill(m_bType, pkVictim);
							m_me->UseArrow(pkArrow, iUseArrow);
						}
					}
					break;

				case SKILL_GIGUNG:
					{
						int iUseArrow = 1;
						if (iUseArrow == m_me->GetArrowAndBow(&pkBow, &pkArrow, iUseArrow))
						{
							m_me->OnMove(true);
							pkVictim->OnMove();

							if (pkVictim->CanBeginFight())
								pkVictim->BeginFight(m_me);

							metin2::log::info( "{} gigung {}", m_me->GetName(), pkVictim->GetName());
							m_me->ComputeSkill(m_bType, pkVictim);
							m_me->UseArrow(pkArrow, iUseArrow);
						}
					}

					break;
				case SKILL_HWAJO:
					{
						int iUseArrow = 1;
						if (iUseArrow == m_me->GetArrowAndBow(&pkBow, &pkArrow, iUseArrow))
						{
							m_me->OnMove(true);
							pkVictim->OnMove();

							if (pkVictim->CanBeginFight())
								pkVictim->BeginFight(m_me);

							metin2::log::info( "{} hwajo {}", m_me->GetName(), pkVictim->GetName());
							m_me->ComputeSkill(m_bType, pkVictim);
							m_me->UseArrow(pkArrow, iUseArrow);
						}
					}

					break;

				case SKILL_HORSE_WILDATTACK_RANGE:
					{
						int iUseArrow = 1;
						if (iUseArrow == m_me->GetArrowAndBow(&pkBow, &pkArrow, iUseArrow))
						{
							m_me->OnMove(true);
							pkVictim->OnMove();

							if (pkVictim->CanBeginFight())
								pkVictim->BeginFight(m_me);

							metin2::log::info( "{} horse_wildattack {}", m_me->GetName(), pkVictim->GetName());
							m_me->ComputeSkill(m_bType, pkVictim);
							m_me->UseArrow(pkArrow, iUseArrow);
						}
					}

					break;

				case SKILL_MARYUNG:
					//case SKILL_GUMHWAN:
				case SKILL_TUSOK:
				case SKILL_BIPABU:
				case SKILL_NOEJEON:
				case SKILL_GEOMPUNG:
				case SKILL_SANGONG:
				case SKILL_MAHWAN:
				case SKILL_PABEOB:
					//case SKILL_CURSE:
					{
						m_me->OnMove(true);
						pkVictim->OnMove();

						if (pkVictim->CanBeginFight())
							pkVictim->BeginFight(m_me);

						metin2::log::info( "{} - Skill {:d} -> {}", m_me->GetName(), m_bType, pkVictim->GetName());
						m_me->ComputeSkill(m_bType, pkVictim);
					}
					break;

				case SKILL_CHAIN:
					{
						m_me->OnMove(true);
						pkVictim->OnMove();

						if (pkVictim->CanBeginFight())
							pkVictim->BeginFight(m_me); 

						metin2::log::info( "{} - Skill {:d} -> {}", m_me->GetName(), m_bType, pkVictim->GetName());
						m_me->ComputeSkill(m_bType, pkVictim);

						// TODO hit multiple targets whoosh whoosh whoosh
					}
					break;

				case SKILL_YONGBI:
					{
						m_me->OnMove(true);
					}
					break;

				default:
					metin2::log::error("CFuncShoot: I don't know this type [{:d}] of range attack.", (int) m_bType);
					break;
			}

			m_bSucceed = true;
		}
};

bool CHARACTER::Shoot(uint8_t bType)
{
	metin2::log::debug( "Shoot {} type {:d} flyTargets.size {:d}", GetName(), bType, m_vec_dwFlyTargets.size());

	if (!CanMove())
	{
		return false;
	}	

	CFuncShoot f(this, bType);

	if (m_dwFlyTargetID != 0)
	{
		f(m_dwFlyTargetID);
		m_dwFlyTargetID = 0;
	}

	f = std::for_each(m_vec_dwFlyTargets.begin(), m_vec_dwFlyTargets.end(), f);
	m_vec_dwFlyTargets.clear();

	return f.m_bSucceed;
}

void CHARACTER::FlyTarget(uint32_t dwTargetVID, int32_t x, int32_t y, uint8_t bHeader)
{
	CHARACTER * pkVictim = CHARACTER_MANAGER::instance().Find(dwTargetVID);
	TPacketGCFlyTargeting pack;

	pack.bHeader	= (bHeader == HEADER_CG_FLY_TARGETING) ? HEADER_GC_FLY_TARGETING : HEADER_GC_ADD_FLY_TARGETING;
	pack.dwShooterVID	= GetVID();

	if (pkVictim)
	{
		pack.dwTargetVID = pkVictim->GetVID();
		pack.x = pkVictim->GetX();
		pack.y = pkVictim->GetY();

		if (bHeader == HEADER_CG_FLY_TARGETING)
			m_dwFlyTargetID = dwTargetVID;
		else
			m_vec_dwFlyTargets.push_back(dwTargetVID);
	}
	else
	{
		pack.dwTargetVID = 0;
		pack.x = x;
		pack.y = y;
	}

	metin2::log::debug( "FlyTarget {} vid {:d} x {:d} y {:d}", GetName(), pack.dwTargetVID, pack.x, pack.y);
	PacketAround(&pack, sizeof(pack), this);
}

CHARACTER * CHARACTER::GetNearestVictim(CHARACTER * pkChr)
{
	if (nullptr == pkChr)
		pkChr = this;

	float fMinDist = 99999.0f;
	CHARACTER * pkVictim = nullptr;

	TDamageMap::iterator it = m_map_kDamage.begin();

	// First filter out those not nearby.
	while (it != m_map_kDamage.end())
	{
		const VID & c_VID = it->first;
		++it;

		CHARACTER * pAttacker = CHARACTER_MANAGER::instance().Find(c_VID);

		if (!pAttacker)
			continue;

		if (pAttacker->IsAffectFlag(AFF_EUNHYUNG) || 
				pAttacker->IsAffectFlag(AFF_INVISIBILITY) ||
				pAttacker->IsAffectFlag(AFF_REVIVE_INVISIBLE))
			continue;

		float fDist = DISTANCE_APPROX(pAttacker->GetX() - pkChr->GetX(), pAttacker->GetY() - pkChr->GetY());

		if (fDist < fMinDist)
		{
			pkVictim = pAttacker;
			fMinDist = fDist;
		}
	}

	return pkVictim;
}

void CHARACTER::SetVictim(CHARACTER * pkVictim)
{
	if (!pkVictim)
	{
		if (0 != (uint32_t)m_kVIDVictim)
			MonsterLog("공격 대상을 해제");

		m_kVIDVictim.Reset();
		battle_end(this);
	}
	else
	{
		if (m_kVIDVictim != pkVictim->GetVID())
			MonsterLog("공격 대상을 설정: %s", pkVictim->GetName());

		m_kVIDVictim = pkVictim->GetVID();
		m_dwLastVictimSetTime = get_dword_time();
	}
}

CHARACTER * CHARACTER::GetVictim() const
{
	return CHARACTER_MANAGER::instance().Find(m_kVIDVictim);
}

CHARACTER * CHARACTER::GetProtege() const // Returns the target to protect
{
	if (m_pkChrStone)
		return m_pkChrStone;

	if (m_pkParty)
		return m_pkParty->GetLeader();

	return nullptr;
}

int CHARACTER::GetAlignment() const
{
	return m_iAlignment;
}

int CHARACTER::GetRealAlignment() const
{
	return m_iRealAlignment;
}

void CHARACTER::ShowAlignment(bool bShow)
{
	if (bShow)
	{
		if (m_iAlignment != m_iRealAlignment)
		{
			m_iAlignment = m_iRealAlignment;
			UpdatePacket();
		}
	}
	else
	{
		if (m_iAlignment != 0)
		{
			m_iAlignment = 0;
			UpdatePacket();
		}
	}
}

void CHARACTER::UpdateAlignment(int iAmount)
{
	bool bShow = false;

	if (m_iAlignment == m_iRealAlignment)
		bShow = true;

	int i = m_iAlignment / 10;

	m_iRealAlignment = std::clamp(m_iRealAlignment + iAmount, -200000, 200000);

	if (bShow)
	{
		m_iAlignment = m_iRealAlignment;

		if (i != m_iAlignment / 10)
			UpdatePacket();
	}
}

void CHARACTER::SetKillerMode(bool isOn)
{
	if ((isOn ? ADD_CHARACTER_STATE_KILLER : 0) == IS_SET(m_bAddChrState, ADD_CHARACTER_STATE_KILLER))
		return;

	if (isOn)
		SET_BIT(m_bAddChrState, ADD_CHARACTER_STATE_KILLER);
	else
		REMOVE_BIT(m_bAddChrState, ADD_CHARACTER_STATE_KILLER);

	m_iKillerModePulse = thecore_pulse();
	UpdatePacket();
	metin2::log::info( "SetKillerMode Update {}[{:d}]", GetName(), GetPlayerID());
}

bool CHARACTER::IsKillerMode() const
{
	return IS_SET(m_bAddChrState, ADD_CHARACTER_STATE_KILLER);
}

void CHARACTER::UpdateKillerMode()
{
	if (!IsKillerMode())
		return;

	int iKillerSeconds = 30;

	if (thecore_pulse() - m_iKillerModePulse >= PASSES_PER_SEC(iKillerSeconds))
		SetKillerMode(false);
}

void CHARACTER::SetPKMode(uint8_t bPKMode)
{
	if (bPKMode >= PK_MODE_MAX_NUM)
		return;

	if (m_bPKMode == bPKMode)
		return;

	if (bPKMode == PK_MODE_GUILD && !GetGuild())
		bPKMode = PK_MODE_FREE;

	m_bPKMode = bPKMode;
	UpdatePacket();

	metin2::log::info( "PK_MODE: {} {:d}", GetName(), m_bPKMode);
}

uint8_t CHARACTER::GetPKMode() const
{
	return m_bPKMode;
}

struct FuncForgetMyAttacker
{
	CHARACTER * m_ch;
	FuncForgetMyAttacker(CHARACTER * ch)
	{
		m_ch = ch;
	}
	void operator()(CEntity * ent)
	{
		if (ent->IsType(ENTITY_CHARACTER))
		{
			CHARACTER * ch = (CHARACTER *) ent;
			if (ch->IsPC())
				return;
			if (ch->m_kVIDVictim == m_ch->GetVID())
				ch->SetVictim(nullptr);
		}
	}
};

struct FuncAggregateMonster
{
	CHARACTER * m_ch;
	FuncAggregateMonster(CHARACTER * ch)
	{
		m_ch = ch;
	}
	void operator()(CEntity * ent)
	{
		if (ent->IsType(ENTITY_CHARACTER))
		{
			CHARACTER * ch = (CHARACTER *) ent;
			if (ch->IsPC())
				return;
			if (!ch->IsMonster())
				return;
			if (ch->GetVictim())
				return;

			if (number(1, 100) <= 50) // Temporarily pull enemies with 50% chance
				if (DISTANCE_APPROX(ch->GetX() - m_ch->GetX(), ch->GetY() - m_ch->GetY()) < 5000)
					if (ch->CanBeginFight())
						ch->BeginFight(m_ch);
		}
	}
};

struct FuncAttractRanger
{
	CHARACTER * m_ch;
	FuncAttractRanger(CHARACTER * ch)
	{
		m_ch = ch;
	}

	void operator()(CEntity * ent)
	{
		if (ent->IsType(ENTITY_CHARACTER))
		{
			CHARACTER * ch = (CHARACTER *) ent;
			if (ch->IsPC())
				return;
			if (!ch->IsMonster())
				return;
			if (ch->GetVictim() && ch->GetVictim() != m_ch)
				return;
			if (ch->GetMobAttackRange() > 150)
			{
				int iNewRange = 150;//(int)(ch->GetMobAttackRange() * 0.2);
				if (iNewRange < 150)
					iNewRange = 150;

				ch->AddAffect(AFFECT_BOW_DISTANCE, POINT_BOW_DISTANCE, iNewRange - ch->GetMobAttackRange(), AFF_NONE, 3*60, 0, false);
			}
		}
	}
};

struct FuncPullMonster
{
	CHARACTER * m_ch;
	int m_iLength;
	FuncPullMonster(CHARACTER * ch, int iLength = 300)
	{
		m_ch = ch;
		m_iLength = iLength;
	}

	void operator()(CEntity * ent)
	{
		if (ent->IsType(ENTITY_CHARACTER))
		{
			CHARACTER * ch = (CHARACTER *) ent;
			if (ch->IsPC())
				return;
			if (!ch->IsMonster())
				return;
			float fDist = DISTANCE_APPROX(m_ch->GetX() - ch->GetX(), m_ch->GetY() - ch->GetY());
			if (fDist > 3000 || fDist < 100)
				return;

			float fNewDist = fDist - m_iLength;
			if (fNewDist < 100) 
				fNewDist = 100;

			float degree = GetDegreeFromPositionXY(ch->GetX(), ch->GetY(), m_ch->GetX(), m_ch->GetY());
			float fx;
			float fy;

			GetDeltaByDegree(degree, fDist - fNewDist, &fx, &fy);
			int32_t tx = (int32_t)(ch->GetX() + fx);
			int32_t ty = (int32_t)(ch->GetY() + fy);

			ch->Sync(tx, ty);
			ch->Goto(tx, ty);
			ch->CalculateMoveDuration();

			ch->SyncPacket();
		}
	}
};

void CHARACTER::ForgetMyAttacker()
{
	SECTREE * pSec = GetSectree();
	if (pSec)
	{
		FuncForgetMyAttacker f(this);
		pSec->ForEachAround(f);
	}
	ReviveInvisible(5);
}

void CHARACTER::AggregateMonster()
{
	SECTREE * pSec = GetSectree();
	if (pSec)
	{
		FuncAggregateMonster f(this);
		pSec->ForEachAround(f);
	}
}

void CHARACTER::AttractRanger()
{
	SECTREE * pSec = GetSectree();
	if (pSec)
	{
		FuncAttractRanger f(this);
		pSec->ForEachAround(f);
	}
}

void CHARACTER::PullMonster()
{
	SECTREE * pSec = GetSectree();
	if (pSec)
	{
		FuncPullMonster f(this);
		pSec->ForEachAround(f);
	}
}

void CHARACTER::UpdateAggrPointEx(CHARACTER * pAttacker, EDamageType type, int dam, CHARACTER::TBattleInfo & info)
{
	// Increases further depending on attack type
	switch (type)
	{
		case DAMAGE_TYPE_NORMAL_RANGE:
			dam = (int) (dam*1.2f);
			break;

		case DAMAGE_TYPE_RANGE:
			dam = (int) (dam*1.5f);
			break;

		case DAMAGE_TYPE_MAGIC:
			dam = (int) (dam*1.2f);
			break;

		default:
			break;
	}

	// Give a bonus if the attacker is the current target.
	if (pAttacker == GetVictim())
		dam = (int) (dam * 1.2f);

	info.iAggro += dam;

	if (info.iAggro < 0)
		info.iAggro = 0;

	if (GetParty() && dam > 0 && type != DAMAGE_TYPE_SPECIAL)
	{
		CParty * pParty = GetParty();

		// Leaders have somewhat stronger influence
		int iPartyAggroDist = dam;

		if (pParty->GetLeaderPID() == GetVID())
			iPartyAggroDist /= 2;
		else
			iPartyAggroDist /= 3;

		pParty->SendMessage(this, PM_AGGRO_INCREASE, iPartyAggroDist, pAttacker->GetVID());
	}

	ChangeVictimByAggro(info.iAggro, pAttacker);
}

void CHARACTER::UpdateAggrPoint(CHARACTER * pAttacker, EDamageType type, int dam)
{
	if (IsDead() || IsStun())
		return;

	TDamageMap::iterator it = m_map_kDamage.find(pAttacker->GetVID());

	if (it == m_map_kDamage.end())
	{
		m_map_kDamage.insert(TDamageMap::value_type(pAttacker->GetVID(), TBattleInfo(0, dam)));
		it = m_map_kDamage.find(pAttacker->GetVID());
	}

	UpdateAggrPointEx(pAttacker, type, dam, it->second);
}

void CHARACTER::ChangeVictimByAggro(int iNewAggro, CHARACTER * pNewVictim)
{
	if (get_dword_time() - m_dwLastVictimSetTime < 3000) // Must wait 3 seconds
		return;

	if (pNewVictim == GetVictim())
	{
		if (m_iMaxAggro < iNewAggro)
		{
			m_iMaxAggro = iNewAggro;
			return;
		}

		// When aggro has decreased
		TDamageMap::iterator it;
		TDamageMap::iterator itFind = m_map_kDamage.end();

		for (it = m_map_kDamage.begin(); it != m_map_kDamage.end(); ++it)
		{
			if (it->second.iAggro > iNewAggro)
			{
				CHARACTER * ch = CHARACTER_MANAGER::instance().Find(it->first);

				if (ch && !ch->IsDead() && DISTANCE_APPROX(ch->GetX() - GetX(), ch->GetY() - GetY()) < 5000)
				{
					itFind = it;
					iNewAggro = it->second.iAggro;
				}
			}
		}

		if (itFind != m_map_kDamage.end())
		{
			m_iMaxAggro = iNewAggro;
			SetVictim(CHARACTER_MANAGER::instance().Find(itFind->first));
			m_dwStateDuration = 1;
		}
	}
	else
	{
		if (m_iMaxAggro < iNewAggro)
		{
			m_iMaxAggro = iNewAggro;
			SetVictim(pNewVictim);
			m_dwStateDuration = 1;
		}
	}
}
}
