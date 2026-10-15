
#include "affect.h"
#include "affect_flag.h"
#include "arena.h"
#include "battle.h"
#include "char.h"
#include "char_manager.h"
#include "config.h"
#include "desc_client.h"
#include "DragonSoul.h"
#include "event.h"
#include "guild.h"
#include "horsename_manager.h"
#include "item.h"
#include "locale.hpp"
#include "packet.h"
#include "skill.h"
#include "typedef.h"
#include "utils.h"

#include <common/length.h>
#include <common/tables.h>
#include <metin2/log/log.h>
#include <libthecore/utils.h>

#include <algorithm>
#include <cstdint>
#include <cstring>

static bool IS_NO_SAVE_AFFECT(uint32_t type) { return (type == AFFECT_WAR_FLAG || type == AFFECT_REVIVE_INVISIBLE || (type >= AFFECT_PREMIUM_START && type <= AFFECT_PREMIUM_END)); }
static bool IS_NO_CLEAR_ON_DEATH_AFFECT(uint32_t type) { return (type == AFFECT_BLOCK_CHAT || (type >= 500 && type < 600)); }

void SendAffectRemovePacket(DESC * d, uint32_t pid, uint32_t type, uint8_t point)
{
	TPacketGCAffectRemove ptoc;
	ptoc.bHeader	= HEADER_GC_AFFECT_REMOVE;
	ptoc.dwType		= type;
	ptoc.bApplyOn	= point;
	d->Packet(&ptoc, sizeof(TPacketGCAffectRemove));

	TPacketGDRemoveAffect ptod;
	ptod.dwPID		= pid;
	ptod.dwType		= type;
	ptod.bApplyOn	= point;
	db_clientdesc->DBPacket(HEADER_GD_REMOVE_AFFECT, 0, &ptod, sizeof(ptod));
}

void SendAffectAddPacket(DESC * d, CAffect * pkAff)
{
	TPacketGCAffectAdd ptoc;
	ptoc.bHeader		= HEADER_GC_AFFECT_ADD;
	ptoc.elem.dwType		= pkAff->dwType;
	ptoc.elem.bApplyOn		= pkAff->bApplyOn;
	ptoc.elem.lApplyValue	= pkAff->lApplyValue;
	ptoc.elem.dwFlag		= pkAff->dwFlag;
	ptoc.elem.lDuration		= pkAff->lDuration;
	ptoc.elem.lSPCost		= pkAff->lSPCost;
	d->Packet(&ptoc, sizeof(TPacketGCAffectAdd));
}
////////////////////////////////////////////////////////////////////
// Affect
CAffect * CHARACTER::FindAffect(uint32_t dwType, uint8_t bApply) const
{
	auto it = m_list_pkAffect.begin();

	while (it != m_list_pkAffect.end())
	{
		CAffect * pkAffect = *it++;

		if (pkAffect->dwType == dwType && (bApply == APPLY_NONE || bApply == pkAffect->bApplyOn))
			return pkAffect;
	}

	return nullptr;
}

EVENTFUNC(affect_event)
{
	char_event_info* info = dynamic_cast<char_event_info*>( event->info );

	if ( info == nullptr )
	{
		metin2::log::error( "affect_event> <Factor> Null pointer" );
		return 0;
	}

	CHARACTER * ch = info->ch;

	if (ch == nullptr) { // <Factor>
		return 0;
	}

	if (!ch->UpdateAffect())
		return 0;
	else
		return metin2::config::server().passes_per_sec; // 1 second
}

bool CHARACTER::UpdateAffect()
{
	// This doesn't really belong in affect_event, but potions are handled here because
	// this is the only thing processed in a 1-second event.
	if (GetPoint(POINT_HP_RECOVERY) > 0)
	{
		if (GetMaxHP() <= GetHP())
		{
			PointChange(POINT_HP_RECOVERY, -GetPoint(POINT_HP_RECOVERY));
		}
		else
		{
			int iVal = 0;

			iVal = std::min(GetPoint(POINT_HP_RECOVERY), GetMaxHP() * 7 / 100);

			PointChange(POINT_HP, iVal);
			PointChange(POINT_HP_RECOVERY, -iVal);
		}
	}

	if (GetPoint(POINT_SP_RECOVERY) > 0)
	{
		if (GetMaxSP() <= GetSP())
			PointChange(POINT_SP_RECOVERY, -GetPoint(POINT_SP_RECOVERY));
		else 
		{
			int iVal;

			iVal = std::min(GetPoint(POINT_SP_RECOVERY), GetMaxSP() * 7 / 100);

			PointChange(POINT_SP, iVal);
			PointChange(POINT_SP_RECOVERY, -iVal);
		}
	}

	if (GetPoint(POINT_HP_RECOVER_CONTINUE) > 0)
	{
		PointChange(POINT_HP, GetPoint(POINT_HP_RECOVER_CONTINUE));
	}

	if (GetPoint(POINT_SP_RECOVER_CONTINUE) > 0)
	{
		PointChange(POINT_SP, GetPoint(POINT_SP_RECOVER_CONTINUE));
	}

	AutoRecoveryItemProcess( AFFECT_AUTO_HP_RECOVERY );
	AutoRecoveryItemProcess( AFFECT_AUTO_SP_RECOVERY );

	// Stamina recovery
	if (GetMaxStamina() > GetStamina())
	{
		int iSec = (get_dword_time() - GetStopTime()) / 3000;
		if (iSec)
			PointChange(POINT_STAMINA, GetMaxStamina()/1);    
	}


	// ProcessAffect returns true if there are no affects.
	if (ProcessAffect())
		if (GetPoint(POINT_HP_RECOVERY) == 0 && GetPoint(POINT_SP_RECOVERY) == 0 && GetStamina() == GetMaxStamina())
		{
			m_pkAffectEvent = nullptr;
			return false;
		}

	return true;
}

void CHARACTER::StartAffectEvent()
{
	if (m_pkAffectEvent)
		return;

	char_event_info* info = AllocEventInfo<char_event_info>();
	info->ch = this;
	m_pkAffectEvent = event_create(affect_event, info, metin2::config::server().passes_per_sec);
	metin2::log::debug( "StartAffectEvent {} {} {}", GetName(), static_cast<const void *>(this), static_cast<const void *>(m_pkAffectEvent.get()));
}

void CHARACTER::ClearAffect(bool bSave)
{
	TAffectFlag afOld = m_afAffectFlag;
	uint16_t	wMovSpd = GetPoint(POINT_MOV_SPEED);
	uint16_t	wAttSpd = GetPoint(POINT_ATT_SPEED);

	auto it = m_list_pkAffect.begin();

	while (it != m_list_pkAffect.end())
	{
		CAffect * pkAff = *it;

		if (bSave)
		{
			if ( IS_NO_CLEAR_ON_DEATH_AFFECT(pkAff->dwType) || IS_NO_SAVE_AFFECT(pkAff->dwType) )
			{
				++it;
				continue;
			}

			if (IsPC())
			{
				SendAffectRemovePacket(GetDesc(), GetPlayerID(), pkAff->dwType, pkAff->bApplyOn);
			}
		}

		ComputeAffect(pkAff, false);

		it = m_list_pkAffect.erase(it);
		delete pkAff;
	}

	if (afOld != m_afAffectFlag ||
			wMovSpd != GetPoint(POINT_MOV_SPEED) ||
			wAttSpd != GetPoint(POINT_ATT_SPEED))
		UpdatePacket();

	CheckMaximumPoints();

	if (m_list_pkAffect.empty())
		event_cancel(&m_pkAffectEvent);
}

int CHARACTER::ProcessAffect()
{
	bool	bDiff	= false;
	CAffect	*pkAff	= nullptr;

	//
	// Premium handling
	//
	for (int i = 0; i <= PREMIUM_MAX_NUM; ++i)
	{
		int aff_idx = i + AFFECT_PREMIUM_START;

		pkAff = FindAffect(aff_idx);

		if (!pkAff)
			continue;

		int remain = GetPremiumRemainSeconds(i);

		if (remain < 0)
		{
			RemoveAffect(aff_idx);
			bDiff = true;
		}
		else
			pkAff->lDuration = remain + 1;
	}

	////////// HAIR_AFFECT
	pkAff = FindAffect(AFFECT_HAIR);
	if (pkAff)
	{
		// IF HAIR_LIMIT_TIME() < CURRENT_TIME()
		if ( this->GetQuestFlag("hair.limit_time") < get_global_time())
		{
			// SET HAIR NORMAL
			this->SetPart(PART_HAIR, 0);
			// REMOVE HAIR AFFECT
			RemoveAffect(AFFECT_HAIR);
		}
		else
		{
			// INCREASE AFFECT DURATION
			++(pkAff->lDuration);
		}
	}
	////////// HAIR_AFFECT
	//

	CHorseNameManager::instance().Validate(this);

	TAffectFlag afOld = m_afAffectFlag;
	int32_t lMovSpd = GetPoint(POINT_MOV_SPEED);
	int32_t lAttSpd = GetPoint(POINT_ATT_SPEED);

	decltype(m_list_pkAffect.begin()) it;

	it = m_list_pkAffect.begin();

	while (it != m_list_pkAffect.end())
	{
		pkAff = *it;

		bool bEnd = false;

		if (pkAff->dwType >= GUILD_SKILL_START && pkAff->dwType <= GUILD_SKILL_END)
		{
			if (!GetGuild() || !GetGuild()->UnderAnyWar())
				bEnd = true;
		}

		if (pkAff->lSPCost > 0)
		{
			if (GetSP() < pkAff->lSPCost)
				bEnd = true;
			else
				PointChange(POINT_SP, -pkAff->lSPCost);
		}

		// AFFECT_DURATION_BUG_FIX
		// Time is reduced even for infinite-duration items.
		// The time is set very large, so this shouldn't matter.
		if ( --pkAff->lDuration <= 0 )
		{
			bEnd = true;
		}
		// END_AFFECT_DURATION_BUG_FIX

		if (bEnd)
		{
			it = m_list_pkAffect.erase(it);
			ComputeAffect(pkAff, false);
			bDiff = true;
			if (IsPC())
			{
				SendAffectRemovePacket(GetDesc(), GetPlayerID(), pkAff->dwType, pkAff->bApplyOn);
			}

			delete pkAff;

			continue;
		}

		++it;
	}

	if (bDiff)
	{
		if (afOld != m_afAffectFlag ||
				lMovSpd != GetPoint(POINT_MOV_SPEED) ||
				lAttSpd != GetPoint(POINT_ATT_SPEED))
		{
			UpdatePacket();
		}

		CheckMaximumPoints();
	}

	if (m_list_pkAffect.empty())
		return true;

	return false;
}

void CHARACTER::SaveAffect()
{
	TPacketGDAddAffect p;

	auto it = m_list_pkAffect.begin();

	while (it != m_list_pkAffect.end())
	{
		CAffect * pkAff = *it++;

		if (IS_NO_SAVE_AFFECT(pkAff->dwType))
			continue;

		metin2::log::debug( "AFFECT_SAVE: {:d} {:d} {:d} {:d}", pkAff->dwType, pkAff->bApplyOn, pkAff->lApplyValue, pkAff->lDuration);

		p.dwPID			= GetPlayerID();
		p.elem.dwType		= pkAff->dwType;
		p.elem.bApplyOn		= pkAff->bApplyOn;
		p.elem.lApplyValue	= pkAff->lApplyValue;
		p.elem.dwFlag		= pkAff->dwFlag;
		p.elem.lDuration	= pkAff->lDuration;
		p.elem.lSPCost		= pkAff->lSPCost;
		db_clientdesc->DBPacket(HEADER_GD_ADD_AFFECT, 0, &p, sizeof(p));
	}
}

EVENTINFO(load_affect_login_event_info)
{
	uint32_t pid;
	uint32_t count;
	char* data;

	load_affect_login_event_info()
	: pid( 0 )
	, count( 0 )
	, data( nullptr )
	{
	}
};

EVENTFUNC(load_affect_login_event)
{
	load_affect_login_event_info* info = dynamic_cast<load_affect_login_event_info*>( event->info );

	if ( info == nullptr )
	{
		metin2::log::error( "load_affect_login_event_info> <Factor> Null pointer" );
		return 0;
	}

	uint32_t dwPID = info->pid;
	CHARACTER * ch = CHARACTER_MANAGER::instance().FindByPID(dwPID);

	if (!ch)
	{
		delete[] info->data;
		return 0;
	}

	DESC * d = ch->GetDesc();

	if (!d)
	{
		delete[] info->data;
		return 0;
	}

	if (d->IsPhase(PHASE_HANDSHAKE) ||
			d->IsPhase(PHASE_LOGIN) ||
			d->IsPhase(PHASE_SELECT) ||
			d->IsPhase(PHASE_DEAD) ||
			d->IsPhase(PHASE_LOADING))
	{
		return PASSES_PER_SEC(1);
	}
	else if (d->IsPhase(PHASE_CLOSE))
	{
		delete[] info->data;
		return 0;
	}
	else if (d->IsPhase(PHASE_GAME))
	{
		metin2::log::debug( "Affect Load by Event");
		ch->LoadAffect(info->count, (TPacketAffectElement*)info->data);
		delete[] info->data;
		return 0;
	}
	else
	{
		metin2::log::error("input_db.cpp:quest_login_event INVALID PHASE pid {:d}", ch->GetPlayerID());
		delete[] info->data;
		return 0;
	}
}

void CHARACTER::LoadAffect(uint32_t dwCount, TPacketAffectElement * pElements)
{

	if (!GetDesc()->IsPhase(PHASE_GAME))
	{
		if (metin2::config::test_server())
			metin2::log::info( "LOAD_AFFECT: Creating Event {} {:d}", GetName(), dwCount);

		load_affect_login_event_info* info = AllocEventInfo<load_affect_login_event_info>();

		info->pid = GetPlayerID();
		info->count = dwCount;
		info->data = new char[sizeof(TPacketAffectElement) * dwCount];
		std::memcpy(info->data, pElements, sizeof(TPacketAffectElement) * dwCount);

		event_create(load_affect_login_event, info, PASSES_PER_SEC(1));

		return;
	}

	ClearAffect(true);

	if (metin2::config::test_server())
		metin2::log::info( "LOAD_AFFECT: {} count {:d}", GetName(), dwCount);

	TAffectFlag afOld = m_afAffectFlag;

	int32_t lMovSpd = GetPoint(POINT_MOV_SPEED);
	int32_t lAttSpd = GetPoint(POINT_ATT_SPEED);

	for (uint32_t i = 0; i < dwCount; ++i, ++pElements)
	{
		// Do not load Mu Yeong Jin.
		if (pElements->dwType == SKILL_MUYEONG)
			continue;

		if (AFFECT_AUTO_HP_RECOVERY == pElements->dwType || AFFECT_AUTO_SP_RECOVERY == pElements->dwType)
		{
			CItem * item = FindItemByID( pElements->dwFlag );

			if (nullptr == item)
				continue;

			item->Lock(true);
		}

		if (pElements->bApplyOn >= POINT_MAX_NUM)
		{
			metin2::log::error("invalid affect data {} ApplyOn {:d} ApplyValue {:d}",
					GetName(), pElements->bApplyOn, pElements->lApplyValue);
			continue;
		}

		if (metin2::config::test_server())
		{
			metin2::log::info( "Load Affect : Affect {} {:d} {:d}", GetName(), pElements->dwType, pElements->bApplyOn );
		}

		CAffect* pkAff = new CAffect{};
		m_list_pkAffect.push_back(pkAff);

		pkAff->dwType		= pElements->dwType;
		pkAff->bApplyOn		= pElements->bApplyOn;
		pkAff->lApplyValue	= pElements->lApplyValue;
		pkAff->dwFlag		= pElements->dwFlag;
		pkAff->lDuration	= pElements->lDuration;
		pkAff->lSPCost		= pElements->lSPCost;

		SendAffectAddPacket(GetDesc(), pkAff);

		ComputeAffect(pkAff, true);
	
	
	}

	if ( CArenaManager::instance().IsArenaMap(GetMapIndex()) == true )
	{
		RemoveGoodAffect();
	}
	
	if (afOld != m_afAffectFlag || lMovSpd != GetPoint(POINT_MOV_SPEED) || lAttSpd != GetPoint(POINT_ATT_SPEED))
	{
		UpdatePacket();
	}

	StartAffectEvent();


	// Load and initialize dragon soul settings
	DragonSoul_Initialize();
}

bool CHARACTER::AddAffect(uint32_t dwType, uint8_t bApplyOn, int32_t lApplyValue, uint32_t dwFlag, int32_t lDuration, int32_t lSPCost, bool bOverride, bool IsCube )
{
	// CHAT_BLOCK
	if (dwType == AFFECT_BLOCK_CHAT && lDuration > 1)
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("운영자 제제로 채팅이 금지 되었습니다."));
	}
	// END_OF_CHAT_BLOCK

	if (lDuration == 0)
	{
		metin2::log::error("Character::AddAffect lDuration == 0 type {:d}", dwType);
		lDuration = 1;
	}

	CAffect * pkAff = nullptr;

	if (IsCube)
		pkAff = FindAffect(dwType,bApplyOn);
	else
		pkAff = FindAffect(dwType);

	if (dwFlag == AFF_STUN)
	{
		if (m_posDest.x != GetX() || m_posDest.y != GetY())
		{
			m_posDest.x = m_posStart.x = GetX();
			m_posDest.y = m_posStart.y = GetY();
			battle_end(this);

			SyncPacket();
		}
	}

	// Overwrite an existing effect
	if (pkAff && bOverride)
	{
		ComputeAffect(pkAff, false); // First remove the effect

		if (GetDesc())
			SendAffectRemovePacket(GetDesc(), GetPlayerID(), pkAff->dwType, pkAff->bApplyOn);
	}
	else
	{
		//
		// Add the new effect
		//
		// NOTE: So multiple effects can be attached with the same type.
		// 
		pkAff = new CAffect{};
		m_list_pkAffect.push_back(pkAff);

	}

	metin2::log::debug( "AddAffect {} type {:d} apply {:d} {:d} flag {:d} duration {:d}", GetName(), dwType, bApplyOn, lApplyValue, dwFlag, lDuration);
	metin2::log::info( "AddAffect {} type {:d} apply {:d} {:d} flag {:d} duration {:d}", GetName(), dwType, bApplyOn, lApplyValue, dwFlag, lDuration);

	pkAff->dwType	= dwType;
	pkAff->bApplyOn	= bApplyOn;
	pkAff->lApplyValue	= lApplyValue;
	pkAff->dwFlag	= dwFlag;
	pkAff->lDuration	= lDuration;
	pkAff->lSPCost	= lSPCost;

	uint16_t wMovSpd = GetPoint(POINT_MOV_SPEED);
	uint16_t wAttSpd = GetPoint(POINT_ATT_SPEED);

	ComputeAffect(pkAff, true);

	if (pkAff->dwFlag || wMovSpd != GetPoint(POINT_MOV_SPEED) || wAttSpd != GetPoint(POINT_ATT_SPEED))
		UpdatePacket();

	StartAffectEvent();

	if (IsPC())
	{
		SendAffectAddPacket(GetDesc(), pkAff);

		if (IS_NO_SAVE_AFFECT(pkAff->dwType))
			return true;

		TPacketGDAddAffect p;
		p.dwPID			= GetPlayerID();
		p.elem.dwType		= pkAff->dwType;
		p.elem.bApplyOn		= pkAff->bApplyOn;
		p.elem.lApplyValue	= pkAff->lApplyValue;
		p.elem.dwFlag		= pkAff->dwFlag;
		p.elem.lDuration	= pkAff->lDuration;
		p.elem.lSPCost		= pkAff->lSPCost;
		db_clientdesc->DBPacket(HEADER_GD_ADD_AFFECT, 0, &p, sizeof(p));
	}

	return true;
}

void CHARACTER::RefreshAffect()
{
	auto it = m_list_pkAffect.begin();

	while (it != m_list_pkAffect.end())
	{
		CAffect * pkAff = *it++;
		ComputeAffect(pkAff, true);
	}
}

void CHARACTER::ComputeAffect(CAffect * pkAff, bool bAdd)
{
	if (bAdd && pkAff->dwType >= GUILD_SKILL_START && pkAff->dwType <= GUILD_SKILL_END)	
	{
		if (!GetGuild())
			return;

		if (!GetGuild()->UnderAnyWar())
			return;
	}

	if (pkAff->dwFlag)
	{
		if (!bAdd)
			m_afAffectFlag.Reset(pkAff->dwFlag);
		else
			m_afAffectFlag.Set(pkAff->dwFlag);
	}

	if (bAdd)
		PointChange(pkAff->bApplyOn, pkAff->lApplyValue);
	else
		PointChange(pkAff->bApplyOn, -pkAff->lApplyValue);

	if (pkAff->dwType == SKILL_MUYEONG)
	{
		if (bAdd)
			StartMuyeongEvent();
		else
			StopMuyeongEvent();
	}
}

bool CHARACTER::RemoveAffect(CAffect * pkAff)
{
	if (!pkAff)
		return false;

	// AFFECT_BUF_FIX
	m_list_pkAffect.remove(pkAff);
	// END_OF_AFFECT_BUF_FIX

	ComputeAffect(pkAff, false);

	// White flag (Baek-gi) bug fix.
	// The white flag bug happens when you cast a buff skill -> transform -> use white flag (AFFECT_REVIVE_INVISIBLE) and attack immediately.
	// The cause: when transforming, buff skill effects are ignored and only the transform effect applies,
	// but attacking right after using the white flag calls RemoveAffect, and ComputePoints then yields transform effect + buff skill effect.
	// We could make ComputePoints ignore buff skill effects while transformed,
	// but ComputePoints is used widely, so we are reluctant to change it much. (Hard to know what side effects would occur.)
	// So only the case where AFFECT_REVIVE_INVISIBLE is removed via RemoveAffect is fixed.
	// The bug does not occur when the white flag effect expires, so do the same as that case.
	//		(See ProcessAffect: when an Affect is removed because it expired, ComputePoints is not called.)
	if (AFFECT_REVIVE_INVISIBLE != pkAff->dwType)
	{
		ComputePoints();
	}
	else // Fix
	{
		UpdatePacket();
	}

	CheckMaximumPoints();

	if (metin2::config::test_server())
		metin2::log::info( "AFFECT_REMOVE: {} (flag {:d} apply: {:d})", GetName(), pkAff->dwFlag, pkAff->bApplyOn);

	if (IsPC())
	{
		SendAffectRemovePacket(GetDesc(), GetPlayerID(), pkAff->dwType, pkAff->bApplyOn);
	}

	delete pkAff;
	return true;
}

bool CHARACTER::RemoveAffect(uint32_t dwType)
{
	// CHAT_BLOCK
	if (dwType == AFFECT_BLOCK_CHAT)
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("채팅 금지가 풀렸습니다."));
	}
	// END_OF_CHAT_BLOCK

	bool flag = false;

	CAffect * pkAff;

	while ((pkAff = FindAffect(dwType)))
	{
		RemoveAffect(pkAff);
		flag = true;
	}

	return flag;
}

bool CHARACTER::IsAffectFlag(uint32_t dwAff) const
{
	return m_afAffectFlag.IsSet(dwAff);
}

void CHARACTER::RemoveGoodAffect()
{
	RemoveAffect(AFFECT_MOV_SPEED);
	RemoveAffect(AFFECT_ATT_SPEED);
	RemoveAffect(AFFECT_STR);
	RemoveAffect(AFFECT_DEX);
	RemoveAffect(AFFECT_INT);
	RemoveAffect(AFFECT_CON);
	RemoveAffect(AFFECT_CHINA_FIREWORK);

	RemoveAffect(SKILL_JEONGWI);
	RemoveAffect(SKILL_GEOMKYUNG);
	RemoveAffect(SKILL_CHUNKEON);
	RemoveAffect(SKILL_EUNHYUNG);
	RemoveAffect(SKILL_GYEONGGONG);
	RemoveAffect(SKILL_GWIGEOM);
	RemoveAffect(SKILL_TERROR);
	RemoveAffect(SKILL_JUMAGAP);
	RemoveAffect(SKILL_MANASHILED);
	RemoveAffect(SKILL_HOSIN);
	RemoveAffect(SKILL_REFLECT);
	RemoveAffect(SKILL_KWAESOK);
	RemoveAffect(SKILL_JEUNGRYEOK);
	RemoveAffect(SKILL_GICHEON);
}

bool CHARACTER::IsGoodAffect(uint8_t bAffectType) const
{
	switch (bAffectType)
	{
		case (AFFECT_MOV_SPEED):
		case (AFFECT_ATT_SPEED):
		case (AFFECT_STR):
		case (AFFECT_DEX):
		case (AFFECT_INT):
		case (AFFECT_CON):
		case (AFFECT_CHINA_FIREWORK):

		case (SKILL_JEONGWI):
		case (SKILL_GEOMKYUNG):
		case (SKILL_CHUNKEON):
		case (SKILL_EUNHYUNG):
		case (SKILL_GYEONGGONG):
		case (SKILL_GWIGEOM):
		case (SKILL_TERROR):
		case (SKILL_JUMAGAP):
		case (SKILL_MANASHILED):
		case (SKILL_HOSIN):
		case (SKILL_REFLECT):
		case (SKILL_KWAESOK):
		case (SKILL_JEUNGRYEOK):
		case (SKILL_GICHEON):
			return true;
	}
	return false;
}

void CHARACTER::RemoveBadAffect()
{
	metin2::log::info( "RemoveBadAffect {}", GetName());
	// Poison
	RemovePoison();
	RemoveFire();

	// Stun           : Value% chance to stun the target for 5 seconds (stars over the head). (1/2 chance to break when hit)               AFF_STUN
	RemoveAffect(AFFECT_STUN);

	// Slow         : Value% chance to slow the target's attack and move speed. Varies with skill mastery when used as a skill   AFF_SLOW
	RemoveAffect(AFFECT_SLOW);

	// Tu Sok Ma Ryeong
	RemoveAffect(SKILL_TUSOK);

	// Curse

	// Pa Beop Sul

	// Faint           : Value% chance to make the target faint. 2 seconds                                                       AFF_FAINT

	// Web       : Value% chance to lower the target's move speed. -40 for 5 seconds                                      AFF_WEB

	// Sleep         : Value% chance to put the target to sleep for 10 seconds. (breaks when hit)                                        AFF_SLEEP

	// Curse           : Value% chance to lower the target's attack and defense. Varies with skill mastery when used as a skill AFF_CURSE

	// Paralysis           : Value% chance to paralyze the target for 4 seconds.                                                     AFF_PARA

	// Bu Dong Bak Bu       : Shaman skill
}

