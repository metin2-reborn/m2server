#include "marriage.h"

#include "char.h"
#include "char_manager.h"
#include "config.h"
#include "desc.h"
#include "desc_client.h"
#include "event.h"
#include "main.h"
#include "p2p.h"
#include "packet.h"
#include "questmanager.h"
#include "sectree_manager.h"
#include "typedef.h"
#include "utils.h"
#include "wedding.h"

#include <common/length.h>
#include <common/tables.h>
#include <metin2/log/log.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <memory>
#include <string>
#include <utility>


namespace marriage
{
	const int MAX_LOVE_GRADE = 4;
	const int MAX_MARRIAGE_UNIQUE_ITEM = 6;

	struct TMarriageItemBonusByGrade
	{
		uint32_t dwVnum;
		int value[MAX_LOVE_GRADE];
	} g_ItemBonus[MAX_MARRIAGE_UNIQUE_ITEM] = {
		{ 71069,	{ 4,	5,	6,	8,  } }, // Piercing increase
		{ 71070,	{ 10,	12,	15,	20, } }, // Experience increase
		{ 71071,	{ 4,	5,	6,	8,  } }, // Critical increase
		{ 71072,	{ -4,	-5,	-6,	-8, } }, // Reduce opponent's attack power
		{ 71073,	{ 20,	25,	30,	40, } }, // Attack power increase (absolute value)
		{ 71074,	{ 12,	16,	20,	30, } }, // Defense increase (absolute value)

		//{ 71069,	1,	2,	3,	6,	8,  }, // Piercing increase
		//{ 71070,	5,	7,	10,	15,	20, }, // Experience increase
		//{ 71071,	1,	2,	3,	6,	8,  }, // Critical increase
		//{ 71072,	5,	10,	15,	20,	30, }, // Redirect damage taken by partner to me
		//{ 71073,	10,	15,	20,	25,	40, }, // Attack power increase (absolute value)
		//{ 71074,	5,	10,	15,	20,	30, }, // Defense increase (absolute value)
	};

	const int MARRIAGE_POINT_PER_DAY = 1;
	const int MARRIAGE_POINT_PER_DAY_FAST = 2;
	void SendLoverInfo(CHARACTER * ch, const std::string& lover_name, int love_point)
	{
		TPacketGCLoverInfo p;

		p.header = HEADER_GC_LOVER_INFO;
		strlcpy(p.name, lover_name.c_str(), sizeof(p.name));
		p.love_point = love_point;
		ch->GetDesc()->Packet(&p, sizeof(p));
	}

	TMarriage::~TMarriage()
	{
		StopNearCheckEvent();
		if (IsOnline())
		{
			ch1->ChatPacket(CHAT_TYPE_COMMAND, "lover_divorce");
			ch2->ChatPacket(CHAT_TYPE_COMMAND, "lover_divorce");
		}
	}

	int TMarriage::GetMarriageGrade()
	{
		int point = std::clamp(GetMarriagePoint(), 50, 100);
		if (point < 65)
			return 0;
		else if (point < 80)
			return 1;
		else if (point < 100)
			return 2;
		return 3;
	}

	int TMarriage::GetMarriagePoint()
	{
		if (test_server)
		{
			int value = quest::CQuestManager::instance().GetEventFlag("lovepoint");
			if (value)
				return std::clamp(value, 0, 100);
		}

		int point_per_day = MARRIAGE_POINT_PER_DAY;
		int max_limit = 30;
		if (IsOnline())
		{
			if (ch1->GetPremiumRemainSeconds(PREMIUM_MARRIAGE_FAST) > 0 || 
					ch2->GetPremiumRemainSeconds(PREMIUM_MARRIAGE_FAST) > 0)
			{
				point_per_day = MARRIAGE_POINT_PER_DAY_FAST;
				max_limit = 40;
			}
		}

		int days = (get_global_time() - marry_time);
		if (test_server)
			days /= 60;
		else
			days /= 86400;

		// Base 50%

		// While using the mandarin duck feather:
		// Effect from days 80%, 8% per day
		// Effect from combat 80%
		// Total 100%

		// When not in use: 
		// Effect from days 60%, 6% per day
		// Effect from combat 60%
		// Total 100%
		return std::min(50 + std::min(days * point_per_day, max_limit) + std::min(love_point / 1000000, max_limit), 100);
	}

	bool TMarriage::IsNear()
	{
		if (!is_married)
			return false;
		if (!IsOnline())
			return false;

		return ch1->GetMapIndex() == ch2->GetMapIndex();

		// Party check removed

		// Distance check removed
	}

	// Love (affection) value
	int TMarriage::GetBonus(uint32_t dwItemVnum, bool bShare, CHARACTER * me)
	{
		if (!is_married)
			return 0;

		// When the partner is not nearby, only one's own effect applies.

		// Find what the item does.
		int iFindedBonusIndex=0;
		{
			for (iFindedBonusIndex = 0; iFindedBonusIndex < MAX_MARRIAGE_UNIQUE_ITEM; ++iFindedBonusIndex)
			{
				if (g_ItemBonus[iFindedBonusIndex].dwVnum == dwItemVnum)
					break;
			}

			if (iFindedBonusIndex == MAX_MARRIAGE_UNIQUE_ITEM)
				return 0;
		}

		if (bShare)
		{
			// Sum the bonuses of both.
			int count = 0;
			if (nullptr != ch1 && ch1->IsEquipUniqueItem(dwItemVnum))
				count ++;
			if (nullptr != ch2 && ch2->IsEquipUniqueItem(dwItemVnum))
				count ++;

			const TMarriageItemBonusByGrade& rkBonus = g_ItemBonus[iFindedBonusIndex];

			if (count>=1)
				return rkBonus.value[GetMarriageGrade()];
			return 0;
		}
		else
		{
			// Count only the partner's
			int count = 0;
			if (me != ch1 && nullptr!= ch1 && ch1->IsEquipUniqueItem(dwItemVnum))
				count ++;
			if (me != ch2 && nullptr!= ch2 && ch2->IsEquipUniqueItem(dwItemVnum))
				count ++;

			const TMarriageItemBonusByGrade& rkBonus = g_ItemBonus[iFindedBonusIndex];

			if (count>=1)
				return rkBonus.value[GetMarriageGrade()];
			return 0;
		}
	}

	void TMarriage::Login(CHARACTER * ch)
	{
		if (ch->GetPlayerID() == m_pid1)
		{
			ch1 = ch;
			if (is_married)
				SendLoverInfo(ch1, name2, GetMarriagePoint());
		}
		else if (ch->GetPlayerID() == m_pid2)
		{
			ch2 = ch;
			if (is_married)
				SendLoverInfo(ch2, name1, GetMarriagePoint());
		}

		// If both are logged in on this process, link the pointers and fire the event
		if (IsOnline())
		{
			ch1->SetMarryPartner(ch2);
			ch2->SetMarryPartner(ch1);

			StartNearCheckEvent();
		}

		// If both are logged in, send the packet.
		if (is_married)
		{
			DESC * d1;
			DESC * d2;
			CCI * pkCCI;

			d1 = ch1 ? ch1->GetDesc() : nullptr;

			if (!d1)
			{
				pkCCI = P2P_MANAGER::instance().FindByPID(m_pid1);

				if (pkCCI)
				{
					d1 = pkCCI->pkDesc;
					d1->SetRelay(pkCCI->szName);
				}
			}

			d2 = ch2 ? ch2->GetDesc() : nullptr;

			if (!d2)
			{
				pkCCI = P2P_MANAGER::instance().FindByPID(m_pid2);

				if (pkCCI)
				{
					d2 = pkCCI->pkDesc;
					d2->SetRelay(pkCCI->szName);
				}
			}

			if (d1 && d2)
			{
				d1->ChatPacket(CHAT_TYPE_COMMAND, "lover_login");
				d2->ChatPacket(CHAT_TYPE_COMMAND, "lover_login");
				metin2::log::info( "lover_login {:d} {:d}", m_pid1, m_pid2);
			}
		}
	}

	void TMarriage::Logout(uint32_t pid)
	{
		if (pid == m_pid1)
			ch1 = nullptr;
		else if (pid == m_pid2)
			ch2 = nullptr;

		if (ch1 || ch2)
		{
			Save();

			if (ch1)
				ch1->SetMarryPartner(nullptr);

			if (ch2)
				ch2->SetMarryPartner(nullptr);

			StopNearCheckEvent();
		}

		if (is_married)
		{
			DESC * d1;
			DESC * d2;
			CCI * pkCCI;

			d1 = ch1 ? ch1->GetDesc() : nullptr;

			if (!d1)
			{
				pkCCI = P2P_MANAGER::instance().FindByPID(m_pid1);

				if (pkCCI)
				{
					d1 = pkCCI->pkDesc;
					d1->SetRelay(pkCCI->szName);
				}
			}

			if (d1 && !g_bShutdown) {
				d1->ChatPacket(CHAT_TYPE_COMMAND, "lover_logout");
			}

			d2 = ch2 ? ch2->GetDesc() : nullptr;

			if (!d2)
			{
				pkCCI = P2P_MANAGER::instance().FindByPID(m_pid2);

				if (pkCCI)
				{
					d2 = pkCCI->pkDesc;
					d2->SetRelay(pkCCI->szName);
				}
			}

			if (d2 && !g_bShutdown) {
				d2->ChatPacket(CHAT_TYPE_COMMAND, "lover_logout");
			}
		}
	}

	void TMarriage::NearCheck()
	{
		if (!is_married)
			return;

		if (!IsOnline())
		{
			StopNearCheckEvent();
			return;
		}
		metin2::log::info( "NearCheck {:d} {:d} {:d} {:d} {:d} {:d}", m_pid1, m_pid2, IsNear(), isLastNear, byLastLovePoint, GetMarriagePoint());

		if (IsNear() && !isLastNear)
		{
			isLastNear = true;
			ch1->ChatPacket(CHAT_TYPE_COMMAND, "lover_near");
			ch2->ChatPacket(CHAT_TYPE_COMMAND, "lover_near");
		}
		else if (!IsNear() && isLastNear)
		{
			isLastNear = false;
			ch1->ChatPacket(CHAT_TYPE_COMMAND, "lover_far");
			ch2->ChatPacket(CHAT_TYPE_COMMAND, "lover_far");
		}

		if (byLastLovePoint != GetMarriagePoint())
		{
			byLastLovePoint = GetMarriagePoint();
			TPacketGCLovePointUpdate p;
			p.header = HEADER_GC_LOVE_POINT_UPDATE;
			p.love_point = byLastLovePoint;

			ch1->GetDesc()->Packet(&p, sizeof(p));
			ch2->GetDesc()->Packet(&p, sizeof(p));
		}
	}

	EVENTINFO(near_check_event_info)
	{
		TMarriage* pMarriage;

		near_check_event_info()
		: pMarriage( nullptr )
		{
		}
	};

	EVENTFUNC(near_check_event)
	{
		near_check_event_info* info = dynamic_cast<near_check_event_info*>( event->info );

		if ( info == nullptr )
		{
			metin2::log::error( "near_check_event> <Factor> Null pointer" );
			return 0;
		}

		TMarriage* pMarriage = info->pMarriage;
		pMarriage->NearCheck();
		return PASSES_PER_SEC(5);
	}

	void TMarriage::StartNearCheckEvent()
	{
		StopNearCheckEvent();

		near_check_event_info* info = AllocEventInfo<near_check_event_info>();
		info->pMarriage = this;
		eventNearCheck = event_create(near_check_event, info, 1);
	}

	void TMarriage::StopNearCheckEvent()
	{
		byLastLovePoint = 0;
		isLastNear = false;
		event_cancel(&eventNearCheck);
	}

	void TMarriage::Save()
	{
		metin2::log::info( "TMarriage::Save() - RequestUpdate.bSave={:d}", bSave);
		if (bSave)
		{
			CManager::instance().RequestUpdate(m_pid1, m_pid2, love_point, is_married);
			bSave = false;
		}
	}

	void TMarriage::SetMarried()
	{ 
		is_married = 1; 
		bSave = true; 
		Save(); 

		if (IsOnline())
		{
			SendLoverInfo(ch1, name2, GetMarriagePoint());
			SendLoverInfo(ch2, name1, GetMarriagePoint());

			ch1->ChatPacket(CHAT_TYPE_COMMAND, "lover_login");
			ch2->ChatPacket(CHAT_TYPE_COMMAND, "lover_login");
		}
	}

	void TMarriage::Update(uint32_t point)
	{
		if (!IsOnline())
			return;

		if (point > 0 && is_married)
		{
			bSave = true;
			love_point += point;

			love_point = std::min( love_point, 2000000000 );

			if (test_server)
			{
				CHARACTER * ch;
				ch = CHARACTER_MANAGER::instance().FindByPID(m_pid1);
				if (ch)
					ch->ChatPacket(CHAT_TYPE_PARTY, "lovepoint bykill %.3g total %d", love_point / 1000000., GetMarriagePoint());
				ch = CHARACTER_MANAGER::instance().FindByPID(m_pid2);
				if (ch)
					ch->ChatPacket(CHAT_TYPE_PARTY, "lovepoint bykill %.3g total %d", love_point / 1000000., GetMarriagePoint());
			}
		}
	}

	void TMarriage::WarpToWeddingMap(uint32_t dwPID)
	{
		if (!pWeddingInfo)
			return;

		CHARACTER * ch = CHARACTER_MANAGER::instance().FindByPID(dwPID);
		if (ch)
		{
			PIXEL_POSITION pos;
			if (!SECTREE_MANAGER::instance().GetRecallPositionByEmpire(pWeddingInfo->dwMapIndex/10000, 0, pos))
			{
				metin2::log::error("cannot get warp position");
				return;
			}
			ch->SaveExitLocation();
			ch->WarpSet(pos.x, pos.y, pWeddingInfo->dwMapIndex);
		}
	}

	void TMarriage::RequestEndWedding()
	{
		if (!pWeddingInfo)
			return;
		CManager::instance().RequestEndWedding(m_pid1, m_pid2);
	}

	CManager::CManager()
	{
	}

	CManager::~CManager()
	{
	}

	bool CManager::IsMarriageUniqueItem(uint32_t dwItemVnum)
	{
		for (int i = 0; i < MAX_MARRIAGE_UNIQUE_ITEM; i++)
		{
			if (g_ItemBonus[i].dwVnum == dwItemVnum)
				return true;
		}
		return false;
	}

	bool CManager::IsMarried(uint32_t dwPlayerID) 
	{ 
		TMarriage* pkMarriageFinded=Get(dwPlayerID);
		if (pkMarriageFinded && pkMarriageFinded->is_married)
			return true;

		return false;
	}

	bool CManager::IsEngaged(uint32_t dwPlayerID) 
	{ 
		TMarriage* pkMarriageFinded=Get(dwPlayerID);
		if (pkMarriageFinded && !pkMarriageFinded->is_married)
			return true;

		return false;
	}

	bool CManager::IsEngagedOrMarried(uint32_t dwPlayerID) 
	{ 
		return Get(dwPlayerID) != nullptr; 
	}

	bool CManager::Initialize()
	{
		return true;
	}

	void Align(uint32_t& dwPID1, uint32_t& dwPID2)
	{
		if (dwPID1 > dwPID2)
			std::swap(dwPID1, dwPID2);
	}

	TMarriage* CManager::Get(uint32_t dwPlayerID)
	{
		auto it = m_MarriageByPID.find(dwPlayerID);

		if (it != m_MarriageByPID.end())
			return it->second;

		return nullptr;
	}

	void CManager::RequestAdd(uint32_t dwPID1, uint32_t dwPID2, const char* szName1, const char* szName2)
	{
		if (dwPID1 > dwPID2)
		{
			std::swap(dwPID1, dwPID2);
			std::swap(szName1, szName2);
		}

		TPacketMarriageAdd p;

		p.dwPID1 = dwPID1;
		p.dwPID2 = dwPID2;
		strlcpy(p.szName1, szName1, sizeof(p.szName1));
		strlcpy(p.szName2, szName2, sizeof(p.szName2));
		db_clientdesc->DBPacket(HEADER_GD_MARRIAGE_ADD, 0, &p, sizeof(p));
	}

	void CManager::Add(uint32_t dwPID1, uint32_t dwPID2, time_t tMarryTime, const char* szName1, const char* szName2)
	{
		if (IsEngagedOrMarried(dwPID1) || IsEngagedOrMarried(dwPID2))
		{
			metin2::log::error("cannot marry already married character. {:d} - {:d}", dwPID1, dwPID2);
			return;
		}

		if (dwPID1 > dwPID2)
		{
			std::swap(dwPID1, dwPID2);
			std::swap(szName1, szName2);
		}

		TMarriage* pMarriage = new TMarriage(dwPID1, dwPID2, 0, tMarryTime, szName1, szName2);
		m_Marriages.insert(pMarriage);
		m_MarriageByPID.insert(std::make_pair(dwPID1, pMarriage));
		m_MarriageByPID.insert(std::make_pair(dwPID2, pMarriage));
		{
			CHARACTER * A = CHARACTER_MANAGER::instance().FindByPID(dwPID1);
			CHARACTER * B = CHARACTER_MANAGER::instance().FindByPID(dwPID2);

			if (A && B)
			{
				// Send the wedding map request
				TPacketWeddingRequest p;
				p.dwPID1 = dwPID1;
				p.dwPID2 = dwPID2;
				db_clientdesc->DBPacket(HEADER_GD_WEDDING_REQUEST, 0, &p, sizeof(p));
			}
		}
	}

	void CManager::RequestUpdate(uint32_t dwPID1, uint32_t dwPID2, int iUpdatePoint, uint8_t byMarried)
	{
		Align(dwPID1, dwPID2);

		TPacketMarriageUpdate p;
		p.dwPID1 = dwPID1;
		p.dwPID2 = dwPID2;
		p.iLovePoint = iUpdatePoint;
		p.byMarried = byMarried;
		db_clientdesc->DBPacket(HEADER_GD_MARRIAGE_UPDATE, 0, &p, sizeof(p));
	}

	void CManager::Update(uint32_t dwPID1, uint32_t dwPID2, int32_t lTotalPoint, uint8_t byMarried)
	{
		TMarriage* pMarriage = Get(dwPID1);

		if (!pMarriage || pMarriage->GetOther(dwPID1) != dwPID2)
		{
			metin2::log::error("not under marriage : {:d} {:d}", dwPID1, dwPID2);
			return;
		}

		pMarriage->love_point = lTotalPoint;
		pMarriage->is_married = byMarried;
	}

	void CManager::RequestRemove(uint32_t dwPID1, uint32_t dwPID2)
	{
		Align(dwPID1, dwPID2);

		TPacketMarriageRemove p;
		p.dwPID1 = dwPID1;
		p.dwPID2 = dwPID2;
		db_clientdesc->DBPacket(HEADER_GD_MARRIAGE_REMOVE, 0, &p, sizeof(p));
	}

	void CManager::Remove(uint32_t dwPID1, uint32_t dwPID2)
	{
		TMarriage* pMarriage = Get(dwPID1);
		if (!pMarriage || pMarriage->GetOther(dwPID1) != dwPID2)
		{
			metin2::log::error("not under marriage : {:d} {:d}", dwPID1, dwPID2);
			return;
		}

		m_Marriages.erase(pMarriage);
		m_MarriageByPID.erase(dwPID1);
		m_MarriageByPID.erase(dwPID2);

		delete pMarriage;
	}

	void CManager::Login(CHARACTER * ch)
	{
		uint32_t pid = ch->GetPlayerID();

		TMarriage* pMarriage = Get(pid);
		if (!pMarriage)
			return;

		pMarriage->Login(ch);
	}

	void CManager::Logout(uint32_t pid)
	{
		TMarriage * pMarriage = Get(pid);

		if (!pMarriage)
			return;

		pMarriage->Logout(pid);
	}

	void CManager::Logout(CHARACTER * ch)
	{
		Logout(ch->GetPlayerID());
	}

	void CManager::WeddingReady(uint32_t dwPID1, uint32_t dwPID2, uint32_t dwMapIndex)
	{
		TMarriage* pMarriage = Get(dwPID1);
		if (!pMarriage || pMarriage->GetOther(dwPID1) != dwPID2)
		{
			metin2::log::error("wrong marriage {:d}, {:d}", dwPID1, dwPID2);
			return;
		}

		if (!pMarriage->pWeddingInfo)
			pMarriage->pWeddingInfo = std::make_unique<TWeddingInfo>();

		pMarriage->pWeddingInfo->dwMapIndex = dwMapIndex;
	}

	void CManager::WeddingStart(uint32_t dwPID1, uint32_t dwPID2)
	{
		TMarriage* pMarriage = Get(dwPID1);
		if (!pMarriage || pMarriage->GetOther(dwPID1) != dwPID2)
		{
			metin2::log::error("wrong marriage {:d}, {:d}", dwPID1, dwPID2);
			return;
		}

		TWeddingInfo * pwi = pMarriage->pWeddingInfo.get();

		if (!pwi)
			return;

		// Must warp the couple
		pMarriage->WarpToWeddingMap(dwPID1);
		pMarriage->WarpToWeddingMap(dwPID2);

		// Must register so the name shows up in the menu window
		m_setWedding.insert(std::make_pair(dwPID1, dwPID2));
	}

	void CManager::WeddingEnd(uint32_t dwPID1, uint32_t dwPID2)
	{
		TMarriage* pMarriage = Get(dwPID1);
		if (!pMarriage || pMarriage->GetOther(dwPID1) != dwPID2)
		{
			metin2::log::error("wrong marriage {:d}, {:d}", dwPID1, dwPID2);
			return;
		}

		if (!pMarriage->pWeddingInfo)
		{
			metin2::log::error("not under wedding {:d}, {:d}", dwPID1, dwPID2);
			return;
		}

		// Must remove from the map
		if (map_allow_find(WEDDING_MAP_INDEX))
			if (!WeddingManager::instance().End(pMarriage->pWeddingInfo->dwMapIndex))
			{
				metin2::log::error("wedding map error: map_index={:d}", pMarriage->pWeddingInfo->dwMapIndex);
				return;
			}

		pMarriage->pWeddingInfo.reset();

		m_setWedding.erase(std::make_pair(dwPID1, dwPID2));
	}

	void CManager::RequestEndWedding(uint32_t dwPID1, uint32_t dwPID2)
	{
		TPacketWeddingEnd p;
		p.dwPID1 = dwPID1;
		p.dwPID2 = dwPID2;

		db_clientdesc->DBPacket(HEADER_GD_WEDDING_END, 0, &p, sizeof(p));
	}
}
