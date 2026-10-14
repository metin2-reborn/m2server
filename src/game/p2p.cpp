#include "p2p.h"

#include "char.h"
#include "char_manager.h"
#include "config.h"
#include "desc.h"
#include "guild_manager.h"
#include "marriage.h"
#include "messenger_manager.h"
#include "packet.h"
#include "party.h"
#include "sectree_manager.h"
#include "typedef.h"

#include <common/length.h>
#include <libthecore/log.h>

#include <cassert>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>

P2P_MANAGER::P2P_MANAGER()
{
	m_iHandleCount = 0;

	memset(m_aiEmpireUserCount, 0, sizeof(m_aiEmpireUserCount));
}

P2P_MANAGER::~P2P_MANAGER()
{
}

void P2P_MANAGER::Boot(DESC * d)
{
	CHARACTER_MANAGER::NAME_MAP & map = CHARACTER_MANAGER::instance().GetPCMap();
	CHARACTER_MANAGER::NAME_MAP::iterator it = map.begin();

	TPacketGGLogin p;

	while (it != map.end())
	{
		CHARACTER * ch = it->second;
		it++;

		p.bHeader = HEADER_GG_LOGIN;
		strlcpy(p.szName, ch->GetName(), sizeof(p.szName));
		p.dwPID = ch->GetPlayerID();
		p.bEmpire = ch->GetEmpire();
		p.lMapIndex = SECTREE_MANAGER::instance().GetMapIndex(ch->GetX(), ch->GetY());
		p.bChannel = g_bChannel;

		d->Packet(&p, sizeof(p));
	}
}

void P2P_MANAGER::FlushOutput()
{
	std::unordered_set<DESC *>::iterator it = m_set_pkPeers.begin();

	while (it != m_set_pkPeers.end())
	{
		DESC * pkDesc = *it++;
		pkDesc->FlushOutput();
	}
}

void P2P_MANAGER::RegisterAcceptor(DESC * d)
{
	sys_log(0, "P2P Acceptor opened (host %s)", d->GetHostName());
	m_set_pkPeers.insert(d);
	Boot(d);
}

void P2P_MANAGER::UnregisterAcceptor(DESC * d)
{
	sys_log(0, "P2P Acceptor closed (host %s)", d->GetHostName());
	EraseUserByDesc(d);
	m_set_pkPeers.erase(d);
}

void P2P_MANAGER::RegisterConnector(DESC * d)
{
	sys_log(0, "P2P Connector opened (host %s)", d->GetHostName());
	m_set_pkPeers.insert(d);
	Boot(d);

	TPacketGGSetup p;
	p.bHeader = HEADER_GG_SETUP;
	p.wPort = p2p_port;
	p.bChannel = g_bChannel;
	d->Packet(&p, sizeof(p));
}

void P2P_MANAGER::UnregisterConnector(DESC * d)
{
	std::unordered_set<DESC *>::iterator it = m_set_pkPeers.find(d);

	if (it != m_set_pkPeers.end())
	{
		sys_log(0, "P2P Connector closed (host %s)", d->GetHostName());
		EraseUserByDesc(d);
		m_set_pkPeers.erase(it);
	}
}

void P2P_MANAGER::EraseUserByDesc(DESC * d)
{
	TCCIMap::iterator it = m_map_pkCCI.begin();

	while (it != m_map_pkCCI.end())
	{
		CCI * pkCCI = it->second;
		it++;

		if (pkCCI->pkDesc == d)
			Logout(pkCCI);
	}
}

void P2P_MANAGER::Send(const void * c_pvData, int iSize, DESC * except)
{
	std::unordered_set<DESC *>::iterator it = m_set_pkPeers.begin();

	while (it != m_set_pkPeers.end())
	{
		DESC * pkDesc = *it++;

		if (except == pkDesc)
			continue;

		pkDesc->Packet(c_pvData, iSize);
	}
}

void P2P_MANAGER::Login(DESC * d, const TPacketGGLogin * p)
{
	CCI* pkCCI = Find(p->szName);

	bool UpdateP2P = false;

	if (nullptr == pkCCI)
	{
		UpdateP2P = true;
		pkCCI = new CCI;

		strlcpy(pkCCI->szName, p->szName, sizeof(pkCCI->szName));

		pkCCI->dwPID = p->dwPID;
		pkCCI->bEmpire = p->bEmpire;

		if (p->bChannel == g_bChannel)
		{
			if (pkCCI->bEmpire < EMPIRE_MAX_NUM)
			{
				++m_aiEmpireUserCount[pkCCI->bEmpire];
			}
			else
			{
				sys_err("LOGIN_EMPIRE_ERROR: %d >= MAX(%d)", pkCCI->bEmpire, EMPIRE_MAX_NUM);
			}
		}

		m_map_pkCCI.insert(std::make_pair(pkCCI->szName, pkCCI));
		m_map_dwPID_pkCCI.insert(std::make_pair(pkCCI->dwPID, pkCCI));
	}

	pkCCI->lMapIndex = p->lMapIndex;
	pkCCI->pkDesc = d;
	pkCCI->bChannel = p->bChannel;
	sys_log(0, "P2P: Login %s", pkCCI->szName);

	CGuildManager::instance().P2PLoginMember(pkCCI->dwPID);
	CPartyManager::instance().P2PLogin(pkCCI->dwPID, pkCCI->szName);

	// The messenger only needs updating when the CCI is created.
	if (UpdateP2P) {
		std::string name(pkCCI->szName);
	    MessengerManager::instance().P2PLogin(name);
	}
}

void P2P_MANAGER::Logout(CCI * pkCCI)
{
	if (pkCCI->bChannel == g_bChannel)
	{
		if (pkCCI->bEmpire < EMPIRE_MAX_NUM)
		{
			--m_aiEmpireUserCount[pkCCI->bEmpire];
			if (m_aiEmpireUserCount[pkCCI->bEmpire] < 0)
			{
				sys_err("m_aiEmpireUserCount[%d] < 0", pkCCI->bEmpire);
			}
		}
		else
		{
			sys_err("LOGOUT_EMPIRE_ERROR: %d >= MAX(%d)", pkCCI->bEmpire, EMPIRE_MAX_NUM);
		}
	}

	std::string name(pkCCI->szName);

	CGuildManager::instance().P2PLogoutMember(pkCCI->dwPID);
	CPartyManager::instance().P2PLogout(pkCCI->dwPID);
	MessengerManager::instance().P2PLogout(name);
	marriage::CManager::instance().Logout(pkCCI->dwPID);

	m_map_pkCCI.erase(name);
	m_map_dwPID_pkCCI.erase(pkCCI->dwPID);
	delete pkCCI;
}

void P2P_MANAGER::Logout(const char * c_pszName)
{
	CCI * pkCCI = Find(c_pszName);

	if (!pkCCI)
		return;

	Logout(pkCCI);
	sys_log(0, "P2P: Logout %s", c_pszName);
}

CCI * P2P_MANAGER::FindByPID(uint32_t pid)
{
	TPIDCCIMap::iterator it = m_map_dwPID_pkCCI.find(pid);
	if (it == m_map_dwPID_pkCCI.end())
		return nullptr;
	return it->second;
}

CCI * P2P_MANAGER::Find(const char * c_pszName)
{
	TCCIMap::const_iterator it;

	it = m_map_pkCCI.find(c_pszName);

	if (it == m_map_pkCCI.end())
		return nullptr;

	return it->second;
}

int P2P_MANAGER::GetCount()
{
	return m_aiEmpireUserCount[1] + m_aiEmpireUserCount[2] + m_aiEmpireUserCount[3];
}

int P2P_MANAGER::GetEmpireUserCount(int idx)
{
	assert(idx < EMPIRE_MAX_NUM);
	return m_aiEmpireUserCount[idx];
}


int P2P_MANAGER::GetDescCount()
{
	return m_set_pkPeers.size();
}

void P2P_MANAGER::GetP2PHostNames(std::string& hostNames)
{
	std::unordered_set<DESC *>::iterator it = m_set_pkPeers.begin();

	std::ostringstream oss(std::ostringstream::out);

	while (it != m_set_pkPeers.end())
	{
		DESC * pkDesc = *it++;

		oss << pkDesc->GetP2PHost() << " " << pkDesc->GetP2PPort() << "\n";

	}
	hostNames += oss.str();
}
