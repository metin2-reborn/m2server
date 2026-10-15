#include "monarch.h"

#include "char.h"
#include "config.h"
#include "desc_client.h"
#include "locale.hpp"
#include "sectree_manager.h"
#include "typedef.h"

#include <common/length.h>
#include <common/tables.h>
#include <metin2/log/log.h>
#include <libthecore/main.h>

#include <cstdint>
#include <cstring>
#include <iterator>


CMonarch::CMonarch()
{
	memset( &m_MonarchInfo, 0, sizeof(m_MonarchInfo) );

	Initialize();
}

CMonarch::~CMonarch()
{
}

bool CMonarch::Initialize()
{
	memset(m_PowerUp, 0, sizeof(m_PowerUp));
	memset(m_DefenseUp, 0, sizeof(m_DefenseUp));
	memset(m_PowerUpCT, 0, sizeof(m_PowerUpCT));
	memset(m_DefenseUpCT, 0, sizeof(m_DefenseUpCT));

	return 0;
}

struct FHealMyEmpire 
{
	uint8_t m_bEmpire;
	void operator () (CEntity * ent)
	{
		if (ent->IsType(ENTITY_CHARACTER))
		{
			CHARACTER * ch = (CHARACTER *) ent;

			if (ch->IsPC() && m_bEmpire == ch->GetEmpire())
			{
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("군주가 내린 축복으로 모든 에너지가 가득 채워집니다"));
				ch->PointChange(POINT_HP, ch->GetMaxHP() - ch->GetHP());
				ch->PointChange(POINT_SP, ch->GetMaxSP() - ch->GetSP());
				ch->EffectPacket(SE_SPUP_BLUE);
				ch->EffectPacket(SE_HPUP_RED);
			}
		}
	}
};

int CMonarch::HealMyEmpire(CHARACTER * ch ,uint32_t price)
{
	uint8_t Empire = ch->GetEmpire();
	uint32_t pid = ch->GetPlayerID();

	metin2::log::info( "HealMyEmpire[{:d}] pid:{:d} price {:d}", pid, Empire, price);

	if (IsMonarch(pid, Empire) == 0)
	{
		if (!ch->IsGM())
		{
			ch->ChatPacket(CHAT_TYPE_INFO ,LC_TEXT("군주의 자격을 가지고 있지 않습니다"));
			metin2::log::error("No Monarch pid {:d} ", pid);
			return 0;
		}
	}

	if (!ch->IsMCOK(CHARACTER::MI_HEAL))
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%d 초 후에 군주의 축복을 사용할 수 있습니다"), ch->GetMCLTime(CHARACTER::MI_HEAL));

		return 0;
	}

	if (!IsMoneyOk(price, Empire)) 
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("국고에 돈이 부족합니다. 현재 : %u 필요금액 : %u"), GetMoney(Empire), price);
		return 0;
	}

	int iMapIndex = ch->GetMapIndex();

	FHealMyEmpire f;
	f.m_bEmpire = Empire;
	SECTREE_MANAGER::instance().for_each(iMapIndex, f);	

	// Send money deduction to DB 
	SendtoDBDecMoney(price, Empire, ch);

	// Set cooldown	
	ch->SetMC(CHARACTER::MI_HEAL);

	if (test_server)
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("[TEST_ONLY]현재 국고 : %d "), GetMoney(Empire) - price);
	return 1;
}

void CMonarch::SetMonarchInfo(MonarchInfo * pInfo)
{
	memcpy(&m_MonarchInfo, pInfo, sizeof(MonarchInfo));
}

bool CMonarch::IsMonarch(uint32_t pid, uint8_t bEmpire)
{
	if (bEmpire >= std::size(m_MonarchInfo.pid))
		return false;

	return (m_MonarchInfo.pid[bEmpire] == pid);
}

bool CMonarch::IsMoneyOk(int price, uint8_t bEmpire)
{
	metin2::log::debug( "GetMoney({:d}), price = ({:d},{:d})", bEmpire, GetMoney(bEmpire), price);
	return (GetMoney(bEmpire) >= price);
}

bool CMonarch::SendtoDBAddMoney(int Money, uint8_t bEmpire, CHARACTER * ch)
{
	if (GetMoney(bEmpire) + Money > 2000000000)
		return false;

	if (GetMoney(bEmpire) + Money < 0)
		return false;

	int nEmpire = bEmpire;	
	db_clientdesc->DBPacketHeader(HEADER_GD_ADD_MONARCH_MONEY, ch->GetDesc()->GetHandle(), sizeof(int) + sizeof(int));
	db_clientdesc->Packet(&nEmpire, sizeof(int));	
	db_clientdesc->Packet(&Money, sizeof(int));	
	return true;
}

bool CMonarch::SendtoDBDecMoney(int Money, uint8_t bEmpire, CHARACTER * ch)
{
	if (bEmpire >= std::size(m_MonarchInfo.money))
		return false;

	if (GetMoney(bEmpire) - Money < 0)
		return false;

	// The actual deduction is processed after the round trip to the server
	int nEmpire = bEmpire;	

	db_clientdesc->DBPacketHeader(HEADER_GD_DEC_MONARCH_MONEY, ch->GetDesc()->GetHandle(), sizeof(int) + sizeof(int));
	db_clientdesc->Packet(&nEmpire, sizeof(int));	
	db_clientdesc->Packet(&Money, sizeof(int));	
	return true;
}

bool CMonarch::AddMoney(int Money, uint8_t bEmpire)
{
	if (bEmpire >= std::size(m_MonarchInfo.money))
		return false;

	// Cannot deposit 2 billion or more
	if (GetMoney(bEmpire) + Money > 2000000000)
		return false;

	m_MonarchInfo.money[bEmpire] += Money;
	return true;
}

bool CMonarch::DecMoney(int Money, uint8_t bEmpire)
{
	if (bEmpire >= std::size(m_MonarchInfo.money))
		return false;

	if (GetMoney(bEmpire) - Money < 0)
		return false;

	m_MonarchInfo.money[bEmpire] -= Money;
	return true;
}

int CMonarch::GetMoney(uint8_t bEmpire)
{
	if (bEmpire >= std::size(m_MonarchInfo.money))
		return 0;

	return m_MonarchInfo.money[bEmpire];
}

MonarchInfo* CMonarch::GetMonarch()
{
	return &m_MonarchInfo;
}

uint32_t CMonarch::GetMonarchPID(uint8_t Empire)
{
	return Empire < std::size(m_MonarchInfo.pid) ? m_MonarchInfo.pid[Empire] : 0;
}

bool CMonarch::IsPowerUp(uint8_t Empire)
{
	return Empire < std::size(m_PowerUp) ? m_PowerUp[Empire] : false;
}

bool CMonarch::IsDefenceUp(uint8_t Empire)
{
	return Empire < std::size(m_DefenseUp) ? m_DefenseUp[Empire] : false;
}

bool CMonarch::CheckPowerUpCT(uint8_t Empire)
{
	if (Empire >= std::size(m_PowerUpCT))
		return false;

	if (m_PowerUpCT[Empire] > thecore_pulse())
	{
		if (test_server)
			metin2::log::info( "[TEST_ONLY] : CheckPowerUpCT CT{:d} Now{:d} 60sec {:d}", m_PowerUpCT[Empire], thecore_pulse(), PASSES_PER_SEC(60 * 10));
		return false;
	}

	return true;
}

bool CMonarch::CheckDefenseUpCT(uint8_t Empire)
{
	if (Empire >= std::size(m_DefenseUpCT))
		return false;

	if (m_DefenseUpCT[Empire] > thecore_pulse())
	{
		if (test_server)
			metin2::log::info( "[TEST_ONLY] : CheckPowerUpCT CT{:d} Now{:d} 60sec {:d}", m_PowerUpCT[Empire], thecore_pulse(), PASSES_PER_SEC(60 * 10));
		return false;
	}

	return true;
}

void CMonarch::PowerUp(uint8_t Empire, bool On)
{
	if (Empire >= std::size(m_PowerUpCT))
		return;

	m_PowerUp[Empire] = On;

	// Monarch Lion's Roar cooldown 
	m_PowerUpCT[Empire] = thecore_pulse() + PASSES_PER_SEC(60 * 10);
}

void CMonarch::DefenseUp(uint8_t Empire, bool On)
{
	if (Empire >= std::size(m_DefenseUpCT))
		return;

	m_DefenseUp[Empire] = On;

	// Monarch Diamond Fist cooldown 
	m_DefenseUpCT[Empire] = thecore_pulse() + PASSES_PER_SEC(60 * 10);
}

bool IsMonarchWarpZone (int map_idx)
{
	// Hungry Ghost Cave, Cave of a Thousand.
	if (map_idx >= 10000)
		map_idx /= 10000;

	switch (map_idx)
	{
	case 301:
	case 302:
	case 303:
	case 304:
		//Dungeon
	case 351: // Red Dragon Fortress
	case 352: // White Dragon Fortress
		return false;
	}

	return (map_idx != 208 && map_idx != 216 && map_idx != 217);
}