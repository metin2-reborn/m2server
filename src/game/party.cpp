#include "party.h"

#include "char.h"
#include "char_manager.h"
#include "config.h"
#include "constants.h"
#include "desc.h"
#include "desc_client.h"
#include "dungeon.h"
#include "event.h"
#include "locale.hpp"
#include "p2p.h"
#include "packet.h"
#include "sectree_manager.h"
#include "typedef.h"
#include "unique_item.h"
#include "utils.h"

#include <common/length.h>
#include <common/tables.h>
#include <metin2/log/log.h>
#include <libthecore/utils.h>

#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>

CPartyManager::CPartyManager()
{
	Initialize();
}

CPartyManager::~CPartyManager()
{
}

void CPartyManager::Initialize()
{
	m_bEnablePCParty = false;
}

void CPartyManager::DeleteAllParty()
{
	TPCPartySet::iterator it = m_set_pkPCParty.begin();

	while (it != m_set_pkPCParty.end())
	{
		DeleteParty(*it);
		it = m_set_pkPCParty.begin();
	}
}

bool CPartyManager::SetParty(CHARACTER * ch)	// Must only be used for PCs!!
{
	TPartyMap::iterator it = m_map_pkParty.find(ch->GetPlayerID());

	if (it == m_map_pkParty.end())
		return false;

	CParty * pParty = it->second;
	pParty->Link(ch);
	return true;
}

void CPartyManager::P2PLogin(uint32_t pid, const char* name)
{
	TPartyMap::iterator it = m_map_pkParty.find(pid);

	if (it == m_map_pkParty.end())
		return;

	it->second->UpdateOnlineState(pid, name);
}
void CPartyManager::P2PLogout(uint32_t pid)
{
	TPartyMap::iterator it = m_map_pkParty.find(pid);

	if (it == m_map_pkParty.end())
		return;

	it->second->UpdateOfflineState(pid);
}

void CPartyManager::P2PJoinParty(uint32_t leader, uint32_t pid, uint8_t role)
{
	TPartyMap::iterator it = m_map_pkParty.find(leader);

	if (it != m_map_pkParty.end())
	{
		it->second->P2PJoin(pid);

		if (role >= PARTY_ROLE_MAX_NUM)
			role = PARTY_ROLE_NORMAL;

		it->second->SetRole(pid, role, true);
	}
	else
	{
		metin2::log::error("No such party with leader [{:d}]", leader);
	}
}

void CPartyManager::P2PQuitParty(uint32_t pid)
{
	TPartyMap::iterator it = m_map_pkParty.find(pid);

	if (it != m_map_pkParty.end())
	{
		it->second->P2PQuit(pid);
	}
	else
	{
		metin2::log::error("No such party with member [{:d}]", pid);
	}
}

CParty * CPartyManager::P2PCreateParty(uint32_t pid)
{
	TPartyMap::iterator it = m_map_pkParty.find(pid);
	if (it != m_map_pkParty.end())
		return it->second;

	CParty * pParty = new CParty;

	m_set_pkPCParty.insert(pParty);

	SetPartyMember(pid, pParty);
	pParty->SetPCParty(true);
	pParty->P2PJoin(pid);

	return pParty;
}

void CPartyManager::P2PDeleteParty(uint32_t pid)
{
	TPartyMap::iterator it = m_map_pkParty.find(pid);

	if (it != m_map_pkParty.end())
	{
		m_set_pkPCParty.erase(it->second);
		delete it->second;
	}
	else
		metin2::log::error("PARTY P2PDeleteParty Cannot find party [{:d}]", pid);
}

CParty * CPartyManager::CreateParty(CHARACTER * pLeader)
{
	if (pLeader->GetParty())
		return pLeader->GetParty();

	CParty * pParty = new CParty;

	if (pLeader->IsPC())
	{
		TPacketPartyCreate p;
		p.dwLeaderPID = pLeader->GetPlayerID();

		db_clientdesc->DBPacket(HEADER_GD_PARTY_CREATE, 0, &p, sizeof(TPacketPartyCreate));

		metin2::log::info( "PARTY: Create {} pid {:d}", pLeader->GetName(), pLeader->GetPlayerID());
		pParty->SetPCParty(true);
		pParty->Join(pLeader->GetPlayerID());

		m_set_pkPCParty.insert(pParty);
	}
	else
	{
		pParty->SetPCParty(false);
		pParty->Join(pLeader->GetVID());
	}

	pParty->Link(pLeader);
	return (pParty);
}

void CPartyManager::DeleteParty(CParty * pParty)
{
	TPacketPartyDelete p;
	p.dwLeaderPID = pParty->GetLeaderPID();

	db_clientdesc->DBPacket(HEADER_GD_PARTY_DELETE, 0, &p, sizeof(TPacketPartyDelete));

	m_set_pkPCParty.erase(pParty);
	delete pParty;
}

void CPartyManager::SetPartyMember(uint32_t dwPID, CParty * pParty)
{
	TPartyMap::iterator it = m_map_pkParty.find(dwPID);

	if (pParty == nullptr)
	{
		if (it != m_map_pkParty.end())
			m_map_pkParty.erase(it);
	}
	else
	{
		if (it != m_map_pkParty.end())
		{
			if (it->second != pParty)
			{
				it->second->Quit(dwPID);
				it->second = pParty;
			}
		}
		else
			m_map_pkParty.insert(TPartyMap::value_type(dwPID, pParty));
	}
}

EVENTINFO(party_update_event_info)
{
	uint32_t pid;

	party_update_event_info()
	: pid( 0 )
	{
	}
};

/////////////////////////////////////////////////////////////////////////////
//
// CParty begin!
//
/////////////////////////////////////////////////////////////////////////////
EVENTFUNC(party_update_event)
{
	party_update_event_info* info = dynamic_cast<party_update_event_info*>( event->info );

	if ( info == nullptr )
	{
		metin2::log::error( "party_update_event> <Factor> Null pointer" );
		return 0;
	}

	uint32_t pid = info->pid;
	CHARACTER * leader = CHARACTER_MANAGER::instance().FindByPID(pid);

	if (leader && leader->GetDesc())
	{
		CParty * pParty = leader->GetParty();

		if (pParty)
			pParty->Update();
	}

	return PASSES_PER_SEC(3);
}

CParty::CParty()
{
	Initialize();
}

CParty::~CParty()
{
	Destroy();
}

void CParty::Initialize()
{
	metin2::log::debug( "Party::Initialize");

	m_iExpDistributionMode = PARTY_EXP_DISTRIBUTION_NON_PARITY;
	m_pkChrExpCentralize = nullptr;

	m_dwLeaderPID = 0;

	m_eventUpdate = nullptr;

	memset(&m_anRoleCount, 0, sizeof(m_anRoleCount));
	memset(&m_anMaxRole, 0, sizeof(m_anMaxRole));
	m_anMaxRole[PARTY_ROLE_LEADER] = 1;
	m_anMaxRole[PARTY_ROLE_NORMAL] = 32;

	m_dwPartyStartTime = get_dword_time();
	m_iLongTimeExpBonus = 0;

	m_dwPartyHealTime = get_dword_time();
	m_bPartyHealReady = false;
	m_bCanUsePartyHeal = false;

	m_iLeadership = 0;
	m_iExpBonus = 0;

	m_itNextOwner = m_memberMap.begin();

	m_iCountNearPartyMember = 0;

	m_pkChrLeader = nullptr;
	m_bPCParty = false;
	m_pkDungeon = nullptr;
	m_pkDungeon_for_Only_party = nullptr;
}


void CParty::Destroy()
{
	metin2::log::debug( "Party::Destroy");

	// If the party was created by a PC, the PID must be removed from the party manager's map.
	if (m_bPCParty)
	{
		for (TMemberMap::iterator it = m_memberMap.begin(); it != m_memberMap.end(); ++it)
			CPartyManager::instance().SetPartyMember(it->first, nullptr);
	}

	event_cancel(&m_eventUpdate); 

	RemoveBonus();

	TMemberMap::iterator it = m_memberMap.begin();

	uint32_t dwTime = get_dword_time();

	while (it != m_memberMap.end())
	{
		TMember & rMember = it->second;
		++it;

		if (rMember.pCharacter)
		{
			if (rMember.pCharacter->GetDesc())
			{
				TPacketGCPartyRemove p;
				p.header = HEADER_GC_PARTY_REMOVE;
				p.pid = rMember.pCharacter->GetPlayerID();
				rMember.pCharacter->GetDesc()->Packet(&p, sizeof(p));
				rMember.pCharacter->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("<파티> 파티가 해산 되었습니다."));
			}
			else
			{
				// For NPCs, start an event that makes it disappear after a while when not in combat.
				rMember.pCharacter->SetLastAttacked(dwTime);
				rMember.pCharacter->StartDestroyWhenIdleEvent();
			}

			rMember.pCharacter->SetParty(nullptr);
		}
	}

	m_memberMap.clear();
	m_itNextOwner = m_memberMap.begin();
	
	if (m_pkDungeon_for_Only_party != nullptr)
	{
		m_pkDungeon_for_Only_party->SetPartyNull();
		m_pkDungeon_for_Only_party = nullptr;
	}
}

void CParty::ChatPacketToAllMember(uint8_t type, const char* format, ...)
{
	char chatbuf[CHAT_MAX_LEN + 1];
	va_list args;

	va_start(args, format);
	vsnprintf(chatbuf, sizeof(chatbuf), format, args);
	va_end(args);

	TMemberMap::iterator it;

	for (it = m_memberMap.begin(); it != m_memberMap.end(); ++it)
	{
		TMember & rMember = it->second;

		if (rMember.pCharacter)
		{
			if (rMember.pCharacter->GetDesc())
			{
				rMember.pCharacter->ChatPacket(type, "%s", chatbuf);
			}
		}
	}
}

uint32_t CParty::GetLeaderPID()
{
	return m_dwLeaderPID;
}

uint32_t CParty::GetMemberCount()
{
	return m_memberMap.size();
}

void CParty::P2PJoin(uint32_t dwPID)
{
	TMemberMap::iterator it = m_memberMap.find(dwPID);

	if (it == m_memberMap.end())
	{
		TMember Member;

		Member.pCharacter	= nullptr;
		Member.bNear		= false;

		if (m_memberMap.empty())
		{
			Member.bRole = PARTY_ROLE_LEADER;
			m_dwLeaderPID = dwPID;
		}
		else
			Member.bRole = PARTY_ROLE_NORMAL;

		if (m_bPCParty)
		{
			CHARACTER * ch = CHARACTER_MANAGER::instance().FindByPID(dwPID);

			if (ch)
			{
				metin2::log::info( "PARTY: Join {} pid {:d} leader {:d}", ch->GetName(), dwPID, m_dwLeaderPID);
				Member.strName = ch->GetName();

				if (Member.bRole == PARTY_ROLE_LEADER)
					m_iLeadership = ch->GetLeadershipSkillLevel();
			}
			else
			{
				CCI * pcci = P2P_MANAGER::instance().FindByPID(dwPID);

				if (!pcci);
				else if (pcci->bChannel == metin2::config::server().channel)
					Member.strName = pcci->szName;
				else
					metin2::log::error("member is not in same channel PID: {:d} channel {:d}, this channel {:d}", dwPID, pcci->bChannel, metin2::config::server().channel);
			}
		}

		metin2::log::debug( "PARTY[{:d}] MemberCountChange {:d} -> {:d}", GetLeaderPID(), GetMemberCount(), GetMemberCount()+1);

		m_memberMap.insert(TMemberMap::value_type(dwPID, Member));

		if (m_memberMap.size() == 1)
			m_itNextOwner = m_memberMap.begin();

		if (m_bPCParty)
		{
			CPartyManager::instance().SetPartyMember(dwPID, this);
			SendPartyJoinOneToAll(dwPID);

			CHARACTER * ch = CHARACTER_MANAGER::instance().FindByPID(dwPID);

			if (ch)
				SendParameter(ch);
		}
	}

	if (m_pkDungeon)
	{
		m_pkDungeon->QuitParty(this);
	}
}

void CParty::Join(uint32_t dwPID)
{
	P2PJoin(dwPID);

	if (m_bPCParty)
	{
		TPacketPartyAdd p;
		p.dwLeaderPID = GetLeaderPID();
		p.dwPID = dwPID;
		p.bState = PARTY_ROLE_NORMAL; // #0000790: [M2EU] CZ crash increase: initialization matters! 
		db_clientdesc->DBPacket(HEADER_GD_PARTY_ADD, 0, &p, sizeof(p));
	}
}

void CParty::P2PQuit(uint32_t dwPID)
{
	TMemberMap::iterator it = m_memberMap.find(dwPID);

	if (it == m_memberMap.end())
		return;

	if (m_bPCParty)
		SendPartyRemoveOneToAll(dwPID);

	if (it == m_itNextOwner)
		IncreaseOwnership();

	if (m_bPCParty)
		RemoveBonusForOne(dwPID);

	CHARACTER * ch = it->second.pCharacter;
	uint8_t bRole = it->second.bRole;

	m_memberMap.erase(it);

	metin2::log::debug( "PARTY[{:d}] MemberCountChange {:d} -> {:d}", GetLeaderPID(), GetMemberCount(), GetMemberCount() - 1);

	if (bRole < PARTY_ROLE_MAX_NUM)
	{
		--m_anRoleCount[bRole];
	}
	else
	{
		metin2::log::error("ROLE_COUNT_QUIT_ERROR: INDEX({:d}) > MAX({:d})", bRole, PARTY_ROLE_MAX_NUM);
	}

	if (ch)
	{
		ch->SetParty(nullptr);
		ComputeRolePoint(ch, bRole, false);
	}

	if (m_bPCParty)
		CPartyManager::instance().SetPartyMember(dwPID, nullptr);

	// If the leader leaves, the party must be disbanded.
	if (bRole == PARTY_ROLE_LEADER)
		CPartyManager::instance().DeleteParty(this);

	// Do not add code below this!!! After DeleteParty above, this no longer exists.
}

void CParty::Quit(uint32_t dwPID)
{
	// Always PC
	P2PQuit(dwPID);

	if (m_bPCParty && dwPID != GetLeaderPID())
	{
		TPacketPartyRemove p;
		p.dwPID = dwPID;
		p.dwLeaderPID = GetLeaderPID();
		db_clientdesc->DBPacket(HEADER_GD_PARTY_REMOVE, 0, &p, sizeof(p));
	}
}

void CParty::Link(CHARACTER * pkChr)
{
	TMemberMap::iterator it;

	if (pkChr->IsPC())
		it = m_memberMap.find(pkChr->GetPlayerID());
	else
		it = m_memberMap.find(pkChr->GetVID());

	if (it == m_memberMap.end())
	{
		metin2::log::error("{} is not member of this party", pkChr->GetName());
		return;
	}

	// Create an update event for player parties
	if (m_bPCParty && !m_eventUpdate)
	{
		party_update_event_info* info = AllocEventInfo<party_update_event_info>();
		info->pid = m_dwLeaderPID;
		m_eventUpdate = event_create(party_update_event, info, PASSES_PER_SEC(3));
	}

	if (it->second.bRole == PARTY_ROLE_LEADER)
		m_pkChrLeader = pkChr;

	metin2::log::debug( "PARTY[{:d}] {} linked to party", GetLeaderPID(), pkChr->GetName());

	it->second.pCharacter = pkChr;
	pkChr->SetParty(this);

	if (pkChr->IsPC())
	{
		if (it->second.strName.empty())
		{
			it->second.strName = pkChr->GetName();
		}

		SendPartyJoinOneToAll(pkChr->GetPlayerID());

		SendPartyJoinAllToOne(pkChr);
		SendPartyLinkOneToAll(pkChr);
		SendPartyLinkAllToOne(pkChr);
		SendPartyInfoAllToOne(pkChr);
		SendPartyInfoOneToAll(pkChr);

		SendParameter(pkChr);

		if (GetDungeon() && GetDungeon()->GetMapIndex() == pkChr->GetMapIndex())
		{
			pkChr->SetDungeon(GetDungeon());
		}

		RequestSetMemberLevel(pkChr->GetPlayerID(), pkChr->GetLevel());

	}
}

void CParty::RequestSetMemberLevel(uint32_t pid, uint8_t level)
{
	TPacketPartySetMemberLevel p;
	p.dwLeaderPID = GetLeaderPID();
	p.dwPID = pid;
	p.bLevel = level;
	db_clientdesc->DBPacket(HEADER_GD_PARTY_SET_MEMBER_LEVEL, 0, &p, sizeof(TPacketPartySetMemberLevel));
}

void CParty::P2PSetMemberLevel(uint32_t pid, uint8_t level)
{
	if (!m_bPCParty)
		return;

	TMemberMap::iterator it;

	metin2::log::info( "PARTY P2PSetMemberLevel leader {:d} pid {:d} level {:d}", GetLeaderPID(), pid, level);

	it = m_memberMap.find(pid);
	if (it != m_memberMap.end())
	{
		it->second.bLevel = level;
	}
}

namespace 
{
	struct FExitDungeon
	{
		void operator()(CHARACTER * ch)
		{
			ch->ExitToSavedLocation();
		}
	};
}

void CParty::Unlink(CHARACTER * pkChr)
{
	TMemberMap::iterator it;

	if (pkChr->IsPC())
		it = m_memberMap.find(pkChr->GetPlayerID());
	else
		it = m_memberMap.find(pkChr->GetVID());

	if (it == m_memberMap.end())
	{
		metin2::log::error("{} is not member of this party", pkChr->GetName());
		return;
	}

	if (pkChr->IsPC())
	{
		SendPartyUnlinkOneToAll(pkChr);
		//SendPartyUnlinkAllToOne(pkChr); // Disconnecting, so no need to send Unlink packets.

		if (it->second.bRole == PARTY_ROLE_LEADER)
		{
			RemoveBonus();

			if (it->second.pCharacter->GetDungeon())
			{
				// TODO: if in a dungeon, the rest leave too
				FExitDungeon f;
				ForEachNearMember(f);
			}
		}
	}

	if (it->second.bRole == PARTY_ROLE_LEADER)
		m_pkChrLeader = nullptr;

	it->second.pCharacter = nullptr;
	pkChr->SetParty(nullptr);
}

void CParty::SendPartyRemoveOneToAll(uint32_t pid)
{
	TMemberMap::iterator it;

	TPacketGCPartyRemove p;
	p.header = HEADER_GC_PARTY_REMOVE;
	p.pid = pid;

	for (it = m_memberMap.begin(); it != m_memberMap.end(); ++it)
	{
		if (it->second.pCharacter && it->second.pCharacter->GetDesc())
			it->second.pCharacter->GetDesc()->Packet(&p, sizeof(p));
	}
}

void CParty::SendPartyJoinOneToAll(uint32_t pid)
{
	const TMember& r = m_memberMap[pid];

	TPacketGCPartyAdd p;

	p.header = HEADER_GC_PARTY_ADD;
	p.pid = pid;
	strlcpy(p.name, r.strName.c_str(), sizeof(p.name));

	for (TMemberMap::iterator it = m_memberMap.begin(); it != m_memberMap.end(); ++it)
	{
		if (it->second.pCharacter && it->second.pCharacter->GetDesc())
			it->second.pCharacter->GetDesc()->Packet(&p, sizeof(p));
	}
}

void CParty::SendPartyJoinAllToOne(CHARACTER * ch)
{
	if (!ch->GetDesc())
		return;

	TPacketGCPartyAdd p;

	p.header = HEADER_GC_PARTY_ADD;
	p.name[CHARACTER_NAME_MAX_LEN] = '\0';

	for (TMemberMap::iterator it = m_memberMap.begin();it!= m_memberMap.end(); ++it)
	{
		p.pid = it->first;
		strlcpy(p.name, it->second.strName.c_str(), sizeof(p.name));
		ch->GetDesc()->Packet(&p, sizeof(p));
	}
}

void CParty::SendPartyUnlinkOneToAll(CHARACTER * ch)
{
	if (!ch->GetDesc())
		return;

	TMemberMap::iterator it;

	TPacketGCPartyLink p;
	p.header = HEADER_GC_PARTY_UNLINK;
	p.pid = ch->GetPlayerID();
	p.vid = (uint32_t)ch->GetVID();

	for (it = m_memberMap.begin();it!= m_memberMap.end(); ++it)
	{
		if (it->second.pCharacter && it->second.pCharacter->GetDesc())
		{
			it->second.pCharacter->GetDesc()->Packet(&p, sizeof(p));
		}
	}
}

void CParty::SendPartyLinkOneToAll(CHARACTER * ch)
{
	if (!ch->GetDesc())
		return;

	TMemberMap::iterator it;

	TPacketGCPartyLink p;
	p.header = HEADER_GC_PARTY_LINK;
	p.vid = ch->GetVID();
	p.pid = ch->GetPlayerID();

	for (it = m_memberMap.begin();it!= m_memberMap.end(); ++it)
	{
		if (it->second.pCharacter && it->second.pCharacter->GetDesc())
		{
			it->second.pCharacter->GetDesc()->Packet(&p, sizeof(p));
		}
	}
}

void CParty::SendPartyLinkAllToOne(CHARACTER * ch)
{
	if (!ch->GetDesc())
		return;

	TMemberMap::iterator it;

	TPacketGCPartyLink p;
	p.header = HEADER_GC_PARTY_LINK;

	for (it = m_memberMap.begin();it!= m_memberMap.end(); ++it)
	{
		if (it->second.pCharacter)
		{
			p.vid = it->second.pCharacter->GetVID();
			p.pid = it->second.pCharacter->GetPlayerID();
			ch->GetDesc()->Packet(&p, sizeof(p));
		}
	}
}

void CParty::SendPartyInfoOneToAll(uint32_t pid)
{
	TMemberMap::iterator it = m_memberMap.find(pid);

	if (it == m_memberMap.end())
		return;

	if (it->second.pCharacter)
	{
		SendPartyInfoOneToAll(it->second.pCharacter);
		return;
	}

	// Data Building
	TPacketGCPartyUpdate p;
	memset(&p, 0, sizeof(p));
	p.header = HEADER_GC_PARTY_UPDATE;
	p.pid = pid;
	p.percent_hp = 255;
	p.role = it->second.bRole;

	for (it = m_memberMap.begin();it!= m_memberMap.end(); ++it)
	{
		if ((it->second.pCharacter) && (it->second.pCharacter->GetDesc()))
		{
			it->second.pCharacter->GetDesc()->Packet(&p, sizeof(p));
		}
	}
}

void CParty::SendPartyInfoOneToAll(CHARACTER * ch)
{
	if (!ch->GetDesc())
		return;

	TMemberMap::iterator it;

	// Data Building
	TPacketGCPartyUpdate p;
	ch->BuildUpdatePartyPacket(p);

	for (it = m_memberMap.begin();it!= m_memberMap.end(); ++it)
	{
		if ((it->second.pCharacter) && (it->second.pCharacter->GetDesc()))
		{
			metin2::log::debug( "PARTY send info {}[{:d}] to {}[{:d}]", ch->GetName(), (uint32_t)ch->GetVID(), it->second.pCharacter->GetName(), (uint32_t)it->second.pCharacter->GetVID());
			it->second.pCharacter->GetDesc()->Packet(&p, sizeof(p));
		}
	}
}

void CParty::SendPartyInfoAllToOne(CHARACTER * ch)
{
	TMemberMap::iterator it;

	TPacketGCPartyUpdate p;

	for (it = m_memberMap.begin(); it != m_memberMap.end(); ++it)
	{
		if (!it->second.pCharacter)
		{
			uint32_t pid = it->first;
			memset(&p, 0, sizeof(p));
			p.header = HEADER_GC_PARTY_UPDATE;
			p.pid = pid;
			p.percent_hp = 255;
			p.role = it->second.bRole;
			ch->GetDesc()->Packet(&p, sizeof(p));
			continue;
		}

		it->second.pCharacter->BuildUpdatePartyPacket(p);
		metin2::log::debug( "PARTY send info {}[{:d}] to {}[{:d}]", it->second.pCharacter->GetName(), (uint32_t)it->second.pCharacter->GetVID(), ch->GetName(), (uint32_t)ch->GetVID());
		ch->GetDesc()->Packet(&p, sizeof(p));
	}
}

void CParty::SendMessage(CHARACTER * ch, uint8_t bMsg, uint32_t dwArg1, uint32_t dwArg2)
{
	if (ch->GetParty() != this)
	{
		metin2::log::error("{} is not member of this party {}", ch->GetName(), static_cast<const void *>(this));
		return;
	}

	switch (bMsg)
	{
		case PM_ATTACK:
			break;

		case PM_RETURN:
			{
				TMemberMap::iterator it = m_memberMap.begin();

				while (it != m_memberMap.end())
				{
					TMember & rMember = it->second;
					++it;

					CHARACTER * pkChr;

					if ((pkChr = rMember.pCharacter) && ch != pkChr)
					{
						uint32_t x = dwArg1 + number(-500, 500);
						uint32_t y = dwArg2 + number(-500, 500);

						pkChr->SetVictim(nullptr);
						pkChr->SetRotationToXY(x, y);

						if (pkChr->Goto(x, y))
						{
							CHARACTER * victim = pkChr->GetVictim();
							metin2::log::info( "{} {} RETURN victim {}", pkChr->GetName(), static_cast<const void *>(pkChr), static_cast<const void *>(victim));
							pkChr->SendMovePacket(FUNC_WAIT, 0, 0, 0, 0);
						}
					}
				}
			}
			break;

		case PM_ATTACKED_BY:	// Attacked, ask the leader for help
			{
				// When there is no leader
				CHARACTER * pkChrVictim = ch->GetVictim();

				if (!pkChrVictim)
					return;

				TMemberMap::iterator it = m_memberMap.begin();

				while (it != m_memberMap.end())
				{
					TMember & rMember = it->second;
					++it;

					CHARACTER * pkChr;

					if ((pkChr = rMember.pCharacter) && ch != pkChr)
					{
						if (pkChr->CanBeginFight())
							pkChr->BeginFight(pkChrVictim);
					}
				}
			}
			break;

		case PM_AGGRO_INCREASE:
			{
				CHARACTER * victim = CHARACTER_MANAGER::instance().Find(dwArg2);

				if (!victim)
					return;

				TMemberMap::iterator it = m_memberMap.begin();

				while (it != m_memberMap.end())
				{
					TMember & rMember = it->second;
					++it;

					CHARACTER * pkChr;

					if ((pkChr = rMember.pCharacter) && ch != pkChr)
					{
						pkChr->UpdateAggrPoint(victim, DAMAGE_TYPE_SPECIAL, dwArg1);
					}
				}
			}
			break;
	}
}

CHARACTER * CParty::GetLeaderCharacter()
{
	return m_memberMap[GetLeaderPID()].pCharacter;
}

bool CParty::SetRole(uint32_t dwPID, uint8_t bRole, bool bSet)
{
	TMemberMap::iterator it = m_memberMap.find(dwPID);

	if (it == m_memberMap.end())
	{
		return false;
	}

	CHARACTER * ch = it->second.pCharacter;

	if (bSet)
	{
		if (m_anRoleCount[bRole] >= m_anMaxRole[bRole])
			return false;

		if (it->second.bRole != PARTY_ROLE_NORMAL)
			return false;

		it->second.bRole = bRole;

		if (ch && GetLeader())
			ComputeRolePoint(ch, bRole, true);

		if (bRole < PARTY_ROLE_MAX_NUM)
		{
			++m_anRoleCount[bRole];
		}
		else
		{
			metin2::log::error("ROLE_COUNT_INC_ERROR: INDEX({:d}) > MAX({:d})", bRole, PARTY_ROLE_MAX_NUM);
		}
	}
	else
	{
		if (it->second.bRole == PARTY_ROLE_LEADER)
			return false;

		if (it->second.bRole == PARTY_ROLE_NORMAL)
			return false;

		it->second.bRole = PARTY_ROLE_NORMAL;

		if (ch && GetLeader())
			ComputeRolePoint(ch, PARTY_ROLE_NORMAL, false);

		if (bRole < PARTY_ROLE_MAX_NUM)
		{
			--m_anRoleCount[bRole];
		}
		else
		{
			metin2::log::error("ROLE_COUNT_DEC_ERROR: INDEX({:d}) > MAX({:d})", bRole, PARTY_ROLE_MAX_NUM);
		}
	}

	SendPartyInfoOneToAll(dwPID);
	return true;
}

uint8_t CParty::GetRole(uint32_t pid)
{
	TMemberMap::iterator it = m_memberMap.find(pid);

	if (it == m_memberMap.end())
		return PARTY_ROLE_NORMAL;
	else
		return it->second.bRole;
}

void CParty::RemoveBonus()
{
	TMemberMap::iterator it;

	for (it = m_memberMap.begin(); it != m_memberMap.end(); ++it)
	{
		CHARACTER * ch;

		if ((ch = it->second.pCharacter))
		{
			ComputeRolePoint(ch, it->second.bRole, false);
		}

		it->second.bNear = false;
	}
}

void CParty::RemoveBonusForOne(uint32_t pid)
{
	TMemberMap::iterator it = m_memberMap.find(pid);

	if (it == m_memberMap.end())
		return;

	CHARACTER * ch;

	if ((ch = it->second.pCharacter))
		ComputeRolePoint(ch, it->second.bRole, false);
}

void CParty::HealParty()
{
	// XXX DELETEME until the client is done
	{
		return;
	}
	if (!m_bPartyHealReady)
		return;

	TMemberMap::iterator it;
	CHARACTER * l = GetLeaderCharacter();

	for (it = m_memberMap.begin(); it != m_memberMap.end(); ++it)
	{
		if (!it->second.pCharacter)
			continue;

		CHARACTER * ch = it->second.pCharacter;

		if (DISTANCE_APPROX(l->GetX()-ch->GetX(), l->GetY()-ch->GetY()) < PARTY_DEFAULT_RANGE)
		{
			ch->PointChange(POINT_HP, ch->GetMaxHP()-ch->GetHP());
			ch->PointChange(POINT_SP, ch->GetMaxSP()-ch->GetSP());
		}
	}

	m_bPartyHealReady = false;
	m_dwPartyHealTime = get_dword_time();
}

void CParty::SummonToLeader(uint32_t pid)
{
	int xy[12][2] = 
	{
		{	250,	0		},
		{	216,	125		},
		{	125,	216		},
		{	0,		250		},
		{	-125,	216		},
		{	-216,	125		},
		{	-250,	0		},
		{	-216,	-125	},
		{	-125,	-216	},
		{	0,		-250	},
		{	125,	-216	},
		{	216,	-125	},
	};

	int n = 0;
	int x[12], y[12];

	SECTREE_MANAGER & s = SECTREE_MANAGER::instance();
	CHARACTER * l = GetLeaderCharacter();

	if (m_memberMap.find(pid) == m_memberMap.end())
	{
		l->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("<파티> 소환하려는 대상을 찾을 수 없습니다."));
		return;
	}

	CHARACTER * ch = m_memberMap[pid].pCharacter;

	if (!ch)
	{
		l->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("<파티> 소환하려는 대상을 찾을 수 없습니다."));
		return;
	}

	if (!ch->CanSummon(m_iLeadership))
	{
		l->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("<파티> 대상을 소환할 수 없습니다."));
		return;
	}

	for (int i = 0; i < 12; ++i)
	{
		PIXEL_POSITION p;

		if (s.GetMovablePosition(l->GetMapIndex(), l->GetX() + xy [i][0], l->GetY() + xy[i][1], p))
		{
			x[n] = p.x;
			y[n] = p.y;
			n++;
		}
	}

	if (n == 0)
		l->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("<파티> 파티원을 현재 위치로 소환할 수 없습니다."));
	else
	{
		int i = number(0, n - 1);
		ch->Show(l->GetMapIndex(), x[i], y[i]);
		ch->Stop();
	}
}

void CParty::IncreaseOwnership()
{
	if (m_memberMap.empty())
	{
		m_itNextOwner = m_memberMap.begin();
		return;
	}

	if (m_itNextOwner == m_memberMap.end())
		m_itNextOwner = m_memberMap.begin();
	else
	{
		m_itNextOwner++;

		if (m_itNextOwner == m_memberMap.end())
			m_itNextOwner = m_memberMap.begin();
	}
}

CHARACTER * CParty::GetNextOwnership(CHARACTER * ch, int32_t x, int32_t y)
{
	if (m_itNextOwner == m_memberMap.end())
		return ch;

	int size = m_memberMap.size();

	while (size-- > 0)
	{
		CHARACTER * pkMember = m_itNextOwner->second.pCharacter;

		if (pkMember && DISTANCE_APPROX(pkMember->GetX() - x, pkMember->GetY() - y) < 3000)
		{
			IncreaseOwnership();
			return pkMember;
		}

		IncreaseOwnership();
	}

	return ch;
}

void CParty::ComputeRolePoint(CHARACTER * ch, uint8_t bRole, bool bAdd)
{
	if (!bAdd)
	{
		ch->PointChange(POINT_PARTY_ATTACKER_BONUS, -ch->GetPoint(POINT_PARTY_ATTACKER_BONUS));
		ch->PointChange(POINT_PARTY_TANKER_BONUS, -ch->GetPoint(POINT_PARTY_TANKER_BONUS));
		ch->PointChange(POINT_PARTY_BUFFER_BONUS, -ch->GetPoint(POINT_PARTY_BUFFER_BONUS));
		ch->PointChange(POINT_PARTY_SKILL_MASTER_BONUS, -ch->GetPoint(POINT_PARTY_SKILL_MASTER_BONUS));
		ch->PointChange(POINT_PARTY_DEFENDER_BONUS, -ch->GetPoint(POINT_PARTY_DEFENDER_BONUS));
		ch->PointChange(POINT_PARTY_HASTE_BONUS, -ch->GetPoint(POINT_PARTY_HASTE_BONUS));
		ch->ComputeBattlePoints();
		return;
	}

	//SKILL_POWER_BY_LEVEL
	float k = (float) ch->GetSkillPowerByLevel( std::min(SKILL_MAX_LEVEL, m_iLeadership ) )/ 100.0f;
	//END_SKILL_POWER_BY_LEVEL

	switch (bRole)
	{
		case PARTY_ROLE_ATTACKER:
			{
				int iBonus = (int) (10 + 60 * k);

				if (ch->GetPoint(POINT_PARTY_ATTACKER_BONUS) != iBonus)
				{
					ch->PointChange(POINT_PARTY_ATTACKER_BONUS, iBonus - ch->GetPoint(POINT_PARTY_ATTACKER_BONUS));
					ch->ComputePoints();
				}
			}
			break;

		case PARTY_ROLE_TANKER:
			{
				int iBonus = (int) (50 + 1450 * k);

				if (ch->GetPoint(POINT_PARTY_TANKER_BONUS) != iBonus)
				{
					ch->PointChange(POINT_PARTY_TANKER_BONUS, iBonus - ch->GetPoint(POINT_PARTY_TANKER_BONUS));
					ch->ComputePoints();
				}
			}
			break;

		case PARTY_ROLE_BUFFER:
			{
				int iBonus = (int) (5 + 45 * k);

				if (ch->GetPoint(POINT_PARTY_BUFFER_BONUS) != iBonus)
				{
					ch->PointChange(POINT_PARTY_BUFFER_BONUS, iBonus - ch->GetPoint(POINT_PARTY_BUFFER_BONUS));
				}
			}
			break;

		case PARTY_ROLE_SKILL_MASTER:
			{
				int iBonus = (int) (25 + 600 * k);

				if (ch->GetPoint(POINT_PARTY_SKILL_MASTER_BONUS) != iBonus)
				{
					ch->PointChange(POINT_PARTY_SKILL_MASTER_BONUS, iBonus - ch->GetPoint(POINT_PARTY_SKILL_MASTER_BONUS));
					ch->ComputePoints();
				}
			}
			break;
		case PARTY_ROLE_HASTE:
			{
				int iBonus = (int) (1+5*k);
				if (ch->GetPoint(POINT_PARTY_HASTE_BONUS) != iBonus)
				{
					ch->PointChange(POINT_PARTY_HASTE_BONUS, iBonus - ch->GetPoint(POINT_PARTY_HASTE_BONUS));
					ch->ComputePoints();
				}
			}
			break;
		case PARTY_ROLE_DEFENDER:
			{
				int iBonus = (int) (5+30*k);
				if (ch->GetPoint(POINT_PARTY_DEFENDER_BONUS) != iBonus)
				{
					ch->PointChange(POINT_PARTY_DEFENDER_BONUS, iBonus - ch->GetPoint(POINT_PARTY_DEFENDER_BONUS));
					ch->ComputePoints();
				}
			}
			break;
	}
}

void CParty::Update()
{
	metin2::log::debug( "PARTY::Update");

	CHARACTER * l = GetLeaderCharacter();

	if (!l)
		return;

	TMemberMap::iterator it;

	int iNearMember = 0;
	bool bResendAll = false;

	for (it = m_memberMap.begin(); it != m_memberMap.end(); ++it)
	{
		CHARACTER * ch = it->second.pCharacter;

		it->second.bNear = false;

		if (!ch)
			continue;

		if (l->GetDungeon())
			it->second.bNear = l->GetDungeon() == ch->GetDungeon();
		else
			it->second.bNear = (DISTANCE_APPROX(l->GetX()-ch->GetX(), l->GetY()-ch->GetY()) < PARTY_DEFAULT_RANGE);

		if (it->second.bNear)
		{
			++iNearMember;
		}
	}

	if (iNearMember <= 1 && !l->GetDungeon())
	{
		for (it = m_memberMap.begin(); it != m_memberMap.end(); ++it)
			it->second.bNear = false;

		iNearMember = 0;
	}

	if (iNearMember != m_iCountNearPartyMember)
	{
		m_iCountNearPartyMember = iNearMember;
		bResendAll = true;
	}

	m_iLeadership = l->GetLeadershipSkillLevel();
	int iNewExpBonus = ComputePartyBonusExpPercent();

	if (m_iExpBonus != iNewExpBonus)
	{
		bResendAll = true;
		m_iExpBonus = iNewExpBonus;
	}

	bool bLongTimeExpBonusChanged = false;

	// After enough time since the party was formed, an experience bonus is granted.
	if (!m_iLongTimeExpBonus && (get_dword_time() - m_dwPartyStartTime > PARTY_ENOUGH_MINUTE_FOR_EXP_BONUS * 60 * 1000))
	{
		bLongTimeExpBonusChanged = true;
		m_iLongTimeExpBonus = 5;
		bResendAll = true;
	}

	for (it = m_memberMap.begin(); it != m_memberMap.end(); ++it)
	{
		CHARACTER * ch = it->second.pCharacter;

		if (!ch)
			continue;

		if (bLongTimeExpBonusChanged && ch->GetDesc())
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("파티의 협동력이 높아져 지금부터 추가 경험치 보너스를 받습니다."));

		bool bNear = it->second.bNear;

		ComputeRolePoint(ch, it->second.bRole, bNear);

		if (bNear)
		{
			if (!bResendAll)
				SendPartyInfoOneToAll(ch);
		}
	}

	// PARTY_ROLE_LIMIT_LEVEL_BUG_FIX
	m_anMaxRole[PARTY_ROLE_ATTACKER]	 = m_iLeadership >= 10 ? 1 : 0;
	m_anMaxRole[PARTY_ROLE_HASTE]	 = m_iLeadership >= 20 ? 1 : 0;
	m_anMaxRole[PARTY_ROLE_TANKER]	 = m_iLeadership >= 20 ? 1 : 0;
	m_anMaxRole[PARTY_ROLE_BUFFER]	 = m_iLeadership >= 25 ? 1 : 0;
	m_anMaxRole[PARTY_ROLE_SKILL_MASTER] = m_iLeadership >= 35 ? 1 : 0;
	m_anMaxRole[PARTY_ROLE_DEFENDER] 	 = m_iLeadership >= 40 ? 1 : 0;
	m_anMaxRole[PARTY_ROLE_ATTACKER]	+= m_iLeadership >= 40 ? 1 : 0;
	// END_OF_PARTY_ROLE_LIMIT_LEVEL_BUG_FIX

	// Party Heal Update
	if (!m_bPartyHealReady)
	{
		if (!m_bCanUsePartyHeal && m_iLeadership >= 18)
			m_dwPartyHealTime = get_dword_time();

		m_bCanUsePartyHeal = m_iLeadership >= 18; // Leadership 18 or higher can use heal.

		// Leadership 40 or higher has a shorter party heal cooldown.
		uint32_t PartyHealCoolTime = (m_iLeadership >= 40) ? PARTY_HEAL_COOLTIME_SHORT * 60 * 1000 : PARTY_HEAL_COOLTIME_LONG * 60 * 1000;

		if (m_bCanUsePartyHeal)
		{
			if (get_dword_time() > m_dwPartyHealTime + PartyHealCoolTime)
			{
				m_bPartyHealReady = true;

				// send heal ready
			}
		}
	}

	if (bResendAll)
	{
		for (TMemberMap::iterator it = m_memberMap.begin(); it != m_memberMap.end(); ++it)
			if (it->second.pCharacter)
				SendPartyInfoOneToAll(it->second.pCharacter);
	}
}

void CParty::UpdateOnlineState(uint32_t dwPID, const char* name)
{
	TMember& r = m_memberMap[dwPID];

	TPacketGCPartyAdd p;

	p.header = HEADER_GC_PARTY_ADD;
	p.pid = dwPID;
	r.strName = name;
	strlcpy(p.name, name, sizeof(p.name));

	for (TMemberMap::iterator it = m_memberMap.begin(); it != m_memberMap.end(); ++it)
	{
		if (it->second.pCharacter && it->second.pCharacter->GetDesc())
			it->second.pCharacter->GetDesc()->Packet(&p, sizeof(p));
	}
}
void CParty::UpdateOfflineState(uint32_t dwPID)
{

	TPacketGCPartyAdd p;
	p.header = HEADER_GC_PARTY_ADD;
	p.pid = dwPID;
	memset(p.name, 0, CHARACTER_NAME_MAX_LEN+1);

	for (TMemberMap::iterator it = m_memberMap.begin(); it != m_memberMap.end(); ++it)
	{
		if (it->second.pCharacter && it->second.pCharacter->GetDesc())
			it->second.pCharacter->GetDesc()->Packet(&p, sizeof(p));
	}
}

int CParty::GetFlag(const std::string& name)
{
	TFlagMap::iterator it = m_map_iFlag.find(name);

	if (it != m_map_iFlag.end())
	{
		return it->second;
	}

	return 0;
}

void CParty::SetFlag(const std::string& name, int value)
{
	TFlagMap::iterator it = m_map_iFlag.find(name);

	if (it == m_map_iFlag.end())
	{
		m_map_iFlag.insert(make_pair(name, value));
	}
	else if (it->second != value)
	{
		it->second = value;
	}
}

void CParty::SetDungeon(CDungeon * pDungeon)
{
	m_pkDungeon = pDungeon;
	m_map_iFlag.clear();
}

CDungeon * CParty::GetDungeon()
{
	return m_pkDungeon;
}

void CParty::SetDungeon_for_Only_party(CDungeon * pDungeon)
{
	m_pkDungeon_for_Only_party = pDungeon;
}

CDungeon * CParty::GetDungeon_for_Only_party()
{
	return m_pkDungeon_for_Only_party;
}


bool CParty::IsPositionNearLeader(CHARACTER * ch)
{
	if (!m_pkChrLeader)
		return false;

	if (DISTANCE_APPROX(ch->GetX() - m_pkChrLeader->GetX(), ch->GetY() - m_pkChrLeader->GetY()) >= PARTY_DEFAULT_RANGE)
		return false;

	return true;
}


int CParty::GetExpBonusPercent()
{
	if (GetNearMemberCount() <= 1)
		return 0;

	return m_iExpBonus + m_iLongTimeExpBonus;
}

uint8_t CParty::CountMemberByVnum(uint32_t dwVnum)
{
	if (m_bPCParty)
		return 0;

	CHARACTER * tch;
	uint8_t bCount = 0;

	TMemberMap::iterator it;

	for (it = m_memberMap.begin(); it != m_memberMap.end(); ++it)
	{
		if (!(tch = it->second.pCharacter))
			continue;

		if (tch->IsPC())
			continue;

		if (tch->GetMobTable().dwVnum == dwVnum)
			++bCount;
	}

	return bCount;
}

void CParty::SendParameter(CHARACTER * ch)
{
	TPacketGCPartyParameter p;

	p.bHeader = HEADER_GC_PARTY_PARAMETER;
	p.bDistributeMode = m_iExpDistributionMode;

	DESC * d = ch->GetDesc();

	if (d)
	{
		d->Packet(&p, sizeof(TPacketGCPartyParameter));
	}
}

void CParty::SendParameterToAll()
{
	if (!m_bPCParty)
		return;

	TMemberMap::iterator it;

	for (it = m_memberMap.begin(); it != m_memberMap.end(); ++it)
		if (it->second.pCharacter)
			SendParameter(it->second.pCharacter);
}

void CParty::SetParameter(int iMode)
{
	if (iMode >= PARTY_EXP_DISTRIBUTION_MAX_NUM)
	{
		metin2::log::error("Invalid exp distribution mode {:d}", iMode);
		return;
	}

	m_iExpDistributionMode = iMode;
	SendParameterToAll();
}

int CParty::GetExpDistributionMode()
{
	return m_iExpDistributionMode;
}

CHARACTER * CParty::GetExpCentralizeCharacter()
{
	return m_pkChrExpCentralize;
}

uint8_t CParty::GetMemberMaxLevel()
{
	uint8_t bMax = 0;

	auto it = m_memberMap.begin();
	while (it!=m_memberMap.end())
	{
		if (!it->second.bLevel)
		{
			++it;
			continue;
		}

		if (!bMax)
			bMax = it->second.bLevel;
		else if (it->second.bLevel)
			bMax = std::max<int>(bMax, it->second.bLevel);
		++it;
	}
	return bMax;
}

int CParty::ComputePartyBonusExpPercent()
{
	if (GetNearMemberCount() <= 1)
		return 0;

	CHARACTER * leader = GetLeaderCharacter();

	int iBonusPartyExpFromItem = 0;

	// UPGRADE_PARTY_BONUS
	int iMemberCount=std::min<int>(8, GetNearMemberCount());

	if (leader && (leader->IsEquipUniqueItem(UNIQUE_ITEM_PARTY_BONUS_EXP) || leader->IsEquipUniqueItem(UNIQUE_ITEM_PARTY_BONUS_EXP_MALL)
		|| leader->IsEquipUniqueItem(UNIQUE_ITEM_PARTY_BONUS_EXP_GIFT) || leader->IsEquipUniqueGroup(10010)))
	{
		// Need to verify the Chinese-side Yukdo application.
		iBonusPartyExpFromItem = 30;
	}

	return iBonusPartyExpFromItem + CHN_aiPartyBonusExpPercentByMemberCount[iMemberCount];
	// END_OF_UPGRADE_PARTY_BONUS
}

