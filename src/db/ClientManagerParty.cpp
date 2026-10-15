// vim:ts=4 sw=4
#include "ClientManager.h"
#include "Peer.h"

#include <common/tables.h>
#include <metin2/log/log.h>

#include <utility>

void CClientManager::QUERY_PARTY_CREATE(CPeer* peer, TPacketPartyCreate* p)
{
	TPartyMap & pm = m_map_pkChannelParty[peer->GetChannel()];

	if (pm.find(p->dwLeaderPID) == pm.end())
	{
		pm.insert(make_pair(p->dwLeaderPID, TPartyMember()));
		ForwardPacket(HEADER_DG_PARTY_CREATE, p, sizeof(TPacketPartyCreate), peer->GetChannel(), peer);
		metin2::log::info( "PARTY Create [{:d}]", p->dwLeaderPID);
	}
	else
	{
		metin2::log::error("PARTY Create - Already exists [{:d}]", p->dwLeaderPID);
	}
}

void CClientManager::QUERY_PARTY_DELETE(CPeer* peer, TPacketPartyDelete* p)
{
	TPartyMap& pm = m_map_pkChannelParty[peer->GetChannel()];
	auto it = pm.find(p->dwLeaderPID);

	if (it == pm.end())
	{
		metin2::log::error("PARTY Delete - Non exists [{:d}]", p->dwLeaderPID);
		return;
	}

	pm.erase(it);
	ForwardPacket(HEADER_DG_PARTY_DELETE, p, sizeof(TPacketPartyDelete), peer->GetChannel(), peer);
	metin2::log::info( "PARTY Delete [{:d}]", p->dwLeaderPID);
}

void CClientManager::QUERY_PARTY_ADD(CPeer* peer, TPacketPartyAdd* p)
{
	TPartyMap & pm = m_map_pkChannelParty[peer->GetChannel()];
	auto it = pm.find(p->dwLeaderPID);

	if (it == pm.end())
	{
		metin2::log::error("PARTY Add - Non exists [{:d}]", p->dwLeaderPID);
		return;
	}

	if (it->second.find(p->dwPID) == it->second.end())
	{
		it->second.insert(std::make_pair(p->dwPID, TPartyInfo()));
		ForwardPacket(HEADER_DG_PARTY_ADD, p, sizeof(TPacketPartyAdd), peer->GetChannel(), peer);
		metin2::log::info( "PARTY Add [{:d}] to [{:d}]", p->dwPID, p->dwLeaderPID);
	}
	else
		metin2::log::error("PARTY Add - Already [{:d}] in party [{:d}]", p->dwPID, p->dwLeaderPID);
}

void CClientManager::QUERY_PARTY_REMOVE(CPeer* peer, TPacketPartyRemove* p)
{
	TPartyMap & pm = m_map_pkChannelParty[peer->GetChannel()];
	auto it = pm.find(p->dwLeaderPID);

	if (it == pm.end())
	{
		metin2::log::error("PARTY Remove - Non exists [{:d}] cannot remove [{:d}]",p->dwLeaderPID, p->dwPID);
		return;
	}

	auto pit = it->second.find(p->dwPID);

	if (pit != it->second.end())
	{
		it->second.erase(pit);
		ForwardPacket(HEADER_DG_PARTY_REMOVE, p, sizeof(TPacketPartyRemove), peer->GetChannel(), peer);
		metin2::log::info( "PARTY Remove [{:d}] to [{:d}]", p->dwPID, p->dwLeaderPID);
	}
	else
		metin2::log::error("PARTY Remove - Cannot find [{:d}] in party [{:d}]", p->dwPID, p->dwLeaderPID);
}

void CClientManager::QUERY_PARTY_STATE_CHANGE(CPeer* peer, TPacketPartyStateChange* p)
{
	TPartyMap & pm = m_map_pkChannelParty[peer->GetChannel()];
	auto it = pm.find(p->dwLeaderPID);

	if (it == pm.end())
	{
		metin2::log::error("PARTY StateChange - Non exists [{:d}] cannot state change [{:d}]",p->dwLeaderPID, p->dwPID);
		return;
	}

	auto pit = it->second.find(p->dwPID);

	if (pit == it->second.end())
	{
		metin2::log::error("PARTY StateChange - Cannot find [{:d}] in party [{:d}]", p->dwPID, p->dwLeaderPID);
		return;
	}

	if (p->bFlag)
		pit->second.bRole = p->bRole;
	else 
		pit->second.bRole = 0;

	ForwardPacket(HEADER_DG_PARTY_STATE_CHANGE, p, sizeof(TPacketPartyStateChange), peer->GetChannel(), peer);
	metin2::log::info( "PARTY StateChange [{:d}] at [{:d}] from {:d} {:d}",p->dwPID, p->dwLeaderPID, p->bRole, p->bFlag);
}

void CClientManager::QUERY_PARTY_SET_MEMBER_LEVEL(CPeer* peer, TPacketPartySetMemberLevel* p)
{
	TPartyMap & pm = m_map_pkChannelParty[peer->GetChannel()];
	auto it = pm.find(p->dwLeaderPID);

	if (it == pm.end())
	{
		metin2::log::error("PARTY SetMemberLevel - Non exists [{:d}] cannot level change [{:d}]",p->dwLeaderPID, p->dwPID);
		return;
	}

	auto pit = it->second.find(p->dwPID);

	if (pit == it->second.end())
	{
		metin2::log::error("PARTY SetMemberLevel - Cannot find [{:d}] in party [{:d}]", p->dwPID, p->dwLeaderPID);
		return;
	}

	pit->second.bLevel = p->bLevel;

	ForwardPacket(HEADER_DG_PARTY_SET_MEMBER_LEVEL, p, sizeof(TPacketPartySetMemberLevel), peer->GetChannel());
	metin2::log::info( "PARTY SetMemberLevel pid [{:d}] level {:d}",p->dwPID, p->bLevel);
}
