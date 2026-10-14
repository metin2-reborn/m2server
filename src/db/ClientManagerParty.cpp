// vim:ts=4 sw=4
#include "ClientManager.h"
#include "Peer.h"

#include <common/tables.h>
#include <libthecore/log.h>

#include <utility>

void CClientManager::QUERY_PARTY_CREATE(CPeer* peer, TPacketPartyCreate* p)
{
	TPartyMap & pm = m_map_pkChannelParty[peer->GetChannel()];

	if (pm.find(p->dwLeaderPID) == pm.end())
	{
		pm.insert(make_pair(p->dwLeaderPID, TPartyMember()));
		ForwardPacket(HEADER_DG_PARTY_CREATE, p, sizeof(TPacketPartyCreate), peer->GetChannel(), peer);
		sys_log(0, "PARTY Create [%u]", p->dwLeaderPID);
	}
	else
	{
		sys_err("PARTY Create - Already exists [%u]", p->dwLeaderPID);
	}
}

void CClientManager::QUERY_PARTY_DELETE(CPeer* peer, TPacketPartyDelete* p)
{
	TPartyMap& pm = m_map_pkChannelParty[peer->GetChannel()];
	auto it = pm.find(p->dwLeaderPID);

	if (it == pm.end())
	{
		sys_err("PARTY Delete - Non exists [%u]", p->dwLeaderPID);
		return;
	}

	pm.erase(it);
	ForwardPacket(HEADER_DG_PARTY_DELETE, p, sizeof(TPacketPartyDelete), peer->GetChannel(), peer);
	sys_log(0, "PARTY Delete [%u]", p->dwLeaderPID);
}

void CClientManager::QUERY_PARTY_ADD(CPeer* peer, TPacketPartyAdd* p)
{
	TPartyMap & pm = m_map_pkChannelParty[peer->GetChannel()];
	auto it = pm.find(p->dwLeaderPID);

	if (it == pm.end())
	{
		sys_err("PARTY Add - Non exists [%u]", p->dwLeaderPID);
		return;
	}

	if (it->second.find(p->dwPID) == it->second.end())
	{
		it->second.insert(std::make_pair(p->dwPID, TPartyInfo()));
		ForwardPacket(HEADER_DG_PARTY_ADD, p, sizeof(TPacketPartyAdd), peer->GetChannel(), peer);
		sys_log(0, "PARTY Add [%u] to [%u]", p->dwPID, p->dwLeaderPID);
	}
	else
		sys_err("PARTY Add - Already [%u] in party [%u]", p->dwPID, p->dwLeaderPID);
}

void CClientManager::QUERY_PARTY_REMOVE(CPeer* peer, TPacketPartyRemove* p)
{
	TPartyMap & pm = m_map_pkChannelParty[peer->GetChannel()];
	auto it = pm.find(p->dwLeaderPID);

	if (it == pm.end())
	{
		sys_err("PARTY Remove - Non exists [%u] cannot remove [%u]",p->dwLeaderPID, p->dwPID);
		return;
	}

	auto pit = it->second.find(p->dwPID);

	if (pit != it->second.end())
	{
		it->second.erase(pit);
		ForwardPacket(HEADER_DG_PARTY_REMOVE, p, sizeof(TPacketPartyRemove), peer->GetChannel(), peer);
		sys_log(0, "PARTY Remove [%u] to [%u]", p->dwPID, p->dwLeaderPID);
	}
	else
		sys_err("PARTY Remove - Cannot find [%u] in party [%u]", p->dwPID, p->dwLeaderPID);
}

void CClientManager::QUERY_PARTY_STATE_CHANGE(CPeer* peer, TPacketPartyStateChange* p)
{
	TPartyMap & pm = m_map_pkChannelParty[peer->GetChannel()];
	auto it = pm.find(p->dwLeaderPID);

	if (it == pm.end())
	{
		sys_err("PARTY StateChange - Non exists [%u] cannot state change [%u]",p->dwLeaderPID, p->dwPID);
		return;
	}

	auto pit = it->second.find(p->dwPID);

	if (pit == it->second.end())
	{
		sys_err("PARTY StateChange - Cannot find [%u] in party [%u]", p->dwPID, p->dwLeaderPID);
		return;
	}

	if (p->bFlag)
		pit->second.bRole = p->bRole;
	else 
		pit->second.bRole = 0;

	ForwardPacket(HEADER_DG_PARTY_STATE_CHANGE, p, sizeof(TPacketPartyStateChange), peer->GetChannel(), peer);
	sys_log(0, "PARTY StateChange [%u] at [%u] from %d %d",p->dwPID, p->dwLeaderPID, p->bRole, p->bFlag);
}

void CClientManager::QUERY_PARTY_SET_MEMBER_LEVEL(CPeer* peer, TPacketPartySetMemberLevel* p)
{
	TPartyMap & pm = m_map_pkChannelParty[peer->GetChannel()];
	auto it = pm.find(p->dwLeaderPID);

	if (it == pm.end())
	{
		sys_err("PARTY SetMemberLevel - Non exists [%u] cannot level change [%u]",p->dwLeaderPID, p->dwPID);
		return;
	}

	auto pit = it->second.find(p->dwPID);

	if (pit == it->second.end())
	{
		sys_err("PARTY SetMemberLevel - Cannot find [%u] in party [%u]", p->dwPID, p->dwLeaderPID);
		return;
	}

	pit->second.bLevel = p->bLevel;

	ForwardPacket(HEADER_DG_PARTY_SET_MEMBER_LEVEL, p, sizeof(TPacketPartySetMemberLevel), peer->GetChannel());
	sys_log(0, "PARTY SetMemberLevel pid [%u] level %d",p->dwPID, p->bLevel);
}
