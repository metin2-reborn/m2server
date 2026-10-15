#include "char.h"
#include "char_manager.h"
#include "config.h"
#include "event.h"
#include "guild.h"
#include "horse_rider.h"
#include "horsename_manager.h"
#include "item.h"
#include "locale.hpp"
#include "packet.h"
#include "questmanager.h"
#include "skill.h"
#include "vector.h"

#include <common/length.h>
#include <common/VnumHelper.h>
#include <metin2/log/log.h>
#include <libthecore/utils.h>

#include <cstdint>
#include <cstring>

namespace metin2::game {

bool CHARACTER::StartRiding()
{
	if (IsDead() == true)
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("쓰러진 상태에서는 말에 탈 수 없습니다."));
		return false;
	}
	
	if (IsPolymorphed())
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("변신 상태에서는 말에 탈 수 없습니다."));
		return false;
	}

	// No horse riding while wearing a tuxedo
	CItem * armor = GetWear(WEAR_BODY);

	if (armor && (armor->GetVnum() >= 11901 && armor->GetVnum() <= 11904))
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("예복을 입은 상태에서 말을 탈 수 없습니다."));
		return false;
	}



	uint32_t dwMountVnum = m_chHorse ? m_chHorse->GetRaceNum() : GetMyHorseVnum();

	if (false == CHorseRider::StartRiding())
	{
		if (GetHorseLevel() <= 0)
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("말을 소유하고 있지 않습니다."));
		else if (GetHorseHealth() <= 0)
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("말이 죽어있는 상태 입니다."));
		else if (GetHorseStamina() <= 0)
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("말의 스테미너가 부족하여 말을 탈 수 없습니다."));

		return false;
	}

	// Remove the summoned horse
	HorseSummon(false);

	MountVnum(dwMountVnum);

	if(metin2::config::test_server())
		metin2::log::info( "Ride Horse : {} ", GetName());

	return true;
}

bool CHARACTER::StopRiding()
{
	if (CHorseRider::StopRiding())
	{
		quest::CQuestManager::instance().Unmount(GetPlayerID());

		if (!IsDead() && !IsStun())
		{
			uint32_t dwOldVnum = GetMountVnum();
			MountVnum(0);

			// [NOTE] Changed so that dismounting summons the horse you were riding
			HorseSummon(true, false, dwOldVnum);
		}
		else
		{
			m_dwMountVnum = 0;
			ComputePoints();
			UpdatePacket();
		}

		PointChange(POINT_ST, 0);
		PointChange(POINT_DX, 0);
		PointChange(POINT_HT, 0);
		PointChange(POINT_IQ, 0);

		return true;
	}

	return false;
}

EVENTFUNC(horse_dead_event)
{
	char_event_info* info = dynamic_cast<char_event_info*>( event->info );

	if ( info == nullptr )
	{
		metin2::log::error( "horse_dead_event> <Factor> Null pointer" );
		return 0;
	}

	// <Factor>
	CHARACTER * ch = info->ch;
	if (ch == nullptr) {
		return 0;
	}
	ch->HorseSummon(false);
	return 0;
}

void CHARACTER::SetRider(CHARACTER * ch)
{
	if (m_chRider)
		m_chRider->ClearHorseInfo();

	m_chRider = ch;

	if (m_chRider)
		m_chRider->SendHorseInfo();
}

CHARACTER * CHARACTER::GetRider() const
{
	return m_chRider;
}


void CHARACTER::HorseSummon(bool bSummon, bool bFromFar, uint32_t dwVnum, const char* pPetName)
{
	if ( bSummon )
	{
		//NOTE : On summon, do nothing if a horse already exists.
		if( m_chHorse != nullptr )
			return;

		if (GetHorseLevel() <= 0)
			return;

		// Fail if already riding something
		if (IsRiding())
			return;

		metin2::log::info( "HorseSummon : {} lv:{:d} bSummon:{:d} fromFar:{:d}", GetName(), GetLevel(), bSummon, bFromFar);

		int32_t x = GetX();
		int32_t y = GetY();

		if (GetHorseHealth() <= 0)
			bFromFar = false;

		if (bFromFar)
		{
			x += (number(0, 1) * 2 - 1) * number(2000, 2500);
			y += (number(0, 1) * 2 - 1) * number(2000, 2500);
		}
		else
		{
			x += number(-100, 100);
			y += number(-100, 100);
		}

		m_chHorse = CHARACTER_MANAGER::instance().SpawnMob(
				(0 == dwVnum) ? GetMyHorseVnum() : dwVnum, 
				GetMapIndex(), 
				x, y,
				GetZ(), false, (int)(GetRotation()+180), false);

		if (!m_chHorse)
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("말 소환에 실패하였습니다."));
			return;
		}

		if (GetHorseHealth() <= 0)
		{
			// Make it lie as if dead
			m_chHorse->SetPosition(POS_DEAD);

			// Make it disappear after a while.
			char_event_info* info = AllocEventInfo<char_event_info>();
			info->ch = this;
			m_chHorse->m_pkDeadEvent = event_create(horse_dead_event, info, PASSES_PER_SEC(60));
		}

		m_chHorse->SetLevel(GetHorseLevel());

		const char* pHorseName = CHorseNameManager::instance().GetHorseName(GetPlayerID());

		if ( pHorseName != nullptr && strlen(pHorseName) != 0 )
		{
			m_chHorse->m_stName = pHorseName;
		}
		else
		{
			m_chHorse->m_stName = GetName();
			m_chHorse->m_stName += LC_TEXT("님의 말");
		}

		if (!m_chHorse->Show(GetMapIndex(), x, y, GetZ()))
		{
			CHARACTER_MANAGER::instance().DestroyCharacter(m_chHorse);
			metin2::log::error("cannot show monster");
			m_chHorse = nullptr;
			return;
		}

		if ((GetHorseHealth() <= 0))
		{
			TPacketGCDead pack;
			pack.header	= HEADER_GC_DEAD;
			pack.vid    = m_chHorse->GetVID();
			PacketAround(&pack, sizeof(pack));
		}

		m_chHorse->SetRider(this);
	}
	else
	{
		if (!m_chHorse)
			return;

		CHARACTER * chHorse = m_chHorse;

		chHorse->SetRider(nullptr); // m_chHorse assign to NULL

		if (!bFromFar)
		{
			CHARACTER_MANAGER::instance().DestroyCharacter(chHorse);
		}
		else
		{
			// Make it disappear while moving away
			chHorse->SetNowWalking(false);
			float fx, fy;
			chHorse->SetRotation(GetDegreeFromPositionXY(chHorse->GetX(), chHorse->GetY(), GetX(), GetY())+180);
			GetDeltaByDegree(chHorse->GetRotation(), 3500, &fx, &fy);
			chHorse->Goto((int32_t)(chHorse->GetX()+fx), (int32_t) (chHorse->GetY()+fy));
			chHorse->SendMovePacket(FUNC_WAIT, 0, 0, 0, 0);
		}

		m_chHorse = nullptr;
	}
}

uint32_t CHARACTER::GetMyHorseVnum() const
{
	int delta = 0;

	if (GetGuild())
	{
		++delta;

		if (GetGuild()->GetMasterPID() == GetPlayerID())
			++delta;
	}

	return c_aHorseStat[GetHorseLevel()].iNPCRace + delta;
}

void CHARACTER::HorseDie()
{
	CHorseRider::HorseDie();
	HorseSummon(false);
}

bool CHARACTER::ReviveHorse()
{
	if (CHorseRider::ReviveHorse())
	{
		HorseSummon(false);
		HorseSummon(true);
		return true;
	}
	return false;
}

void CHARACTER::ClearHorseInfo()
{
	if (!IsHorseRiding())
	{
		ChatPacket(CHAT_TYPE_COMMAND, "hide_horse_state");

		m_bSendHorseLevel = 0;
		m_bSendHorseHealthGrade = 0;
		m_bSendHorseStaminaGrade = 0;
	}

	m_chHorse = nullptr;

}

void CHARACTER::SendHorseInfo()
{
	if (m_chHorse || IsHorseRiding())
	{
		int iHealthGrade;
		int iStaminaGrade;
		/*
		   HP	
3: 70% < ~ <= 100%
2: 30% < ~ <= 70%
1:  0% < ~ <= 30%
0: dead

STM

3: 71% < ~ <= 100%
2: 31% < ~ <= 70%
1: 10% < ~ <= 30%
0:	 ~ <= 10%
		 */
		if (GetHorseHealth() == 0)
			iHealthGrade = 0;
		else if (GetHorseHealth() * 10 <= GetHorseMaxHealth() * 3) 
			iHealthGrade = 1;
		else if (GetHorseHealth() * 10 <= GetHorseMaxHealth() * 7) 
			iHealthGrade = 2;
		else
			iHealthGrade = 3;

		if (GetHorseStamina() * 10 <= GetHorseMaxStamina())
			iStaminaGrade = 0;
		else if (GetHorseStamina() * 10 <= GetHorseMaxStamina() * 3) 
			iStaminaGrade = 1;
		else if (GetHorseStamina() * 10 <= GetHorseMaxStamina() * 7) 
			iStaminaGrade = 2;
		else
			iStaminaGrade = 3;

		if (m_bSendHorseLevel != GetHorseLevel() || 
				m_bSendHorseHealthGrade != iHealthGrade || 
				m_bSendHorseStaminaGrade != iStaminaGrade)
		{
			ChatPacket(CHAT_TYPE_COMMAND, "horse_state %d %d %d", GetHorseLevel(), iHealthGrade, iStaminaGrade);

			// FIX : If you skip the code below by returning at the top of the function to hide the "horse status buff" icon on the client,
			// a terrible bug appears that summons horses infinitely.. The exact cause hasn't been investigated, so unknown.
			m_bSendHorseLevel = GetHorseLevel();
			m_bSendHorseHealthGrade = iHealthGrade;
			m_bSendHorseStaminaGrade = iStaminaGrade;
		}
	}
}
		
bool CHARACTER::CanUseHorseSkill()
{
	if(IsRiding())
	{
		if (GetHorseGrade() == 3)
			return true;
		else
			return false;

		if(GetMountVnum())
		{
			if (GetMountVnum() >= 20209 && GetMountVnum() <= 20212)
				return true;

			//Ramadan black horse
			if (CMobVnumHelper::IsRamadanBlackHorse(GetMountVnum()))
				return true;
		}
		else
			return false;

	}

	return false;
}

void CHARACTER::SetHorseLevel(int iLevel)
{
	CHorseRider::SetHorseLevel(iLevel);
	SetSkillLevel(SKILL_HORSE, GetHorseLevel());
}
}
