#include "desc_manager.h"

#include "char.h"
#include "config.h"
#include "crc32.h"
#include "desc.h"
#include "desc_client.h"
#include "desc_p2p.h"
#include "ip_ban.h"
#include "p2p.h"
#include "packet.h"
#include "typedef.h"
#include "utils.h"

#include <common/length.h>
#include <libnet/connection.h>
#include <libnet/io.h>
#include <metin2/log/log.h>
#include <libthecore/utils.h>

#include <algorithm>
#include <climits>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <utility>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

namespace metin2::game {

DESC_MANAGER::DESC_MANAGER() : m_bDestroyed(false)
{
	Initialize();
	//NOTE : what on earth is calling Initialize at the end of Destroy..-_-; really 

}

DESC_MANAGER::~DESC_MANAGER()
{
	Destroy();
}

void DESC_MANAGER::Initialize()
{
	m_iSocketsConnected = 0;
	m_iHandleCount = 0;
	m_iLocalUserCount = 0;
	memset(m_aiEmpireUserCount, 0, sizeof(m_aiEmpireUserCount));
}

void DESC_MANAGER::Destroy()
{
	if (m_bDestroyed) {
		return;
	}
	m_bDestroyed = true;

	DESC_SET::iterator i = m_set_pkDesc.begin();

	while (i != m_set_pkDesc.end())
	{
		DESC * d = *i;
		DESC_SET::iterator ci = i;
		++i;

		if (d->GetType() == DESC_TYPE_CONNECTOR)
			continue;

		if (d->IsPhase(PHASE_P2P))
			continue;

		DestroyDesc(d, false);
		m_set_pkDesc.erase(ci);
	}

	i = m_set_pkDesc.begin();

	while (i != m_set_pkDesc.end())
	{
		DESC * d = *i;
		DESC_SET::iterator ci = i;
		++i;

		DestroyDesc(d, false);
		m_set_pkDesc.erase(ci);
	}

	m_set_pkClientDesc.clear();

	m_map_loginName.clear();
	m_map_handle.clear();

	Initialize();
}

uint32_t DESC_MANAGER::CreateHandshake()
{
	char crc_buf[8];
	crc_t crc;
	DESC_HANDSHAKE_MAP::iterator it;

RETRY:
	do
	{
		uint32_t val = thecore_random() % (1024 * 1024);

		*(uint32_t *) (crc_buf    ) = val;
		*((uint32_t *) crc_buf + 1) = get_global_time();

		crc = GetCRC32(crc_buf, 8);
		it = m_map_handshake.find(crc);
	}
	while (it != m_map_handshake.end());

	if (crc == 0)
		goto RETRY;

	return (crc);
}

DESC * DESC_MANAGER::AcceptDesc(net::tcp::socket socket)
{
	auto connection = net::connection::create(std::move(socket), MAX_CLIENT_INPUT, MAX_CLIENT_OUTPUT);
	const char * host = connection->remote_address().c_str();

	if (metin2::config::is_auth())
	{
		in_addr addr{};

		if (inet_pton(AF_INET, host, &addr) == 1 && IsBanIP(addr))
		{
			metin2::log::info( "connection from {} was banned.", host);
			connection->close();
			return nullptr;
		}
	}

	if (m_iSocketsConnected >= MAX_ALLOW_USER)
	{
		metin2::log::error("max connection reached. MAX_ALLOW_USER = {:d}", MAX_ALLOW_USER);
		connection->close();
		return nullptr;
	}

	DESC * newd = new DESC;
	crc_t handshake = CreateHandshake();

	if (!newd->Setup(std::move(connection), ++m_iHandleCount, handshake))
	{
		newd->Destroy();
		delete newd;
		return nullptr;
	}

	m_map_handshake.insert(DESC_HANDSHAKE_MAP::value_type(handshake, newd));
	m_map_handle.insert(DESC_HANDLE_MAP::value_type(newd->GetHandle(), newd));

	m_set_pkDesc.insert(newd);
	++m_iSocketsConnected;
	return (newd);
}

DESC * DESC_MANAGER::AcceptP2PDesc(net::tcp::socket socket)
{
	DESC_P2P * pkDesc = new DESC_P2P;

	if (!pkDesc->Setup(net::connection::create(std::move(socket), MAX_SERVER_INPUT, MAX_SERVER_OUTPUT)))
	{     
		metin2::log::error("DESC_MANAGER::AcceptP2PDesc : Setup failed");
		pkDesc->Destroy();
		delete pkDesc;
		return nullptr;
	}

	m_set_pkDesc.insert(pkDesc);
	++m_iSocketsConnected;

	metin2::log::info( "DESC_MANAGER::AcceptP2PDesc  {}", pkDesc->GetHostName());
	P2P_MANAGER::instance().RegisterAcceptor(pkDesc);
	return (pkDesc);
}

void DESC_MANAGER::ConnectAccount(const std::string& login, DESC * d)
{
metin2::log::debug( "BBBB ConnectAccount({})", login.c_str());
	m_map_loginName.insert(DESC_LOGINNAME_MAP::value_type(login,d));
}

void DESC_MANAGER::DisconnectAccount(const std::string& login)
{
metin2::log::debug( "BBBB DisConnectAccount({})", login.c_str());
	m_map_loginName.erase(login);
}

void DESC_MANAGER::DestroyDesc(DESC * d, bool bEraseFromSet)
{
	if (bEraseFromSet)
		m_set_pkDesc.erase(d);

	if (d->GetHandshake())
		m_map_handshake.erase(d->GetHandshake());

	if (d->GetHandle() != 0)
	{
		m_map_handle.erase(d->GetHandle());
		m_map_handle_random_key.erase(d->GetHandle());
	}
	else
		m_set_pkClientDesc.erase((CLIENT_DESC *) d);

	// Explicit call to the virtual function Destroy()
	d->Destroy();

	delete d;
	--m_iSocketsConnected;
}

void DESC_MANAGER::DestroyClosed()
{
	DESC_SET::iterator i = m_set_pkDesc.begin();

	while (i != m_set_pkDesc.end())
	{
		DESC * d = *i;
		DESC_SET::iterator ci = i;
		++i;

		if (d->IsPhase(PHASE_CLOSE))
		{
			if (d->GetType() == DESC_TYPE_CONNECTOR)
			{
				CLIENT_DESC * client_desc = (CLIENT_DESC *)d;

				if (client_desc->IsRetryWhenClosed())
				{
					client_desc->Reset();
					continue;
				}
			}

			DestroyDesc(d, false);
			m_set_pkDesc.erase(ci);
		}
	}
}

DESC * DESC_MANAGER::FindByLoginName(const std::string& login)
{
	DESC_LOGINNAME_MAP::iterator it = m_map_loginName.find(login);

	if (m_map_loginName.end() == it)
		return nullptr;

	return (it->second); 
}

DESC * DESC_MANAGER::FindByHandle(uint32_t handle)
{
	DESC_HANDLE_MAP::iterator it = m_map_handle.find(handle);

	if (m_map_handle.end() == it)
		return nullptr;

	return (it->second); 
}

const DESC_MANAGER::DESC_SET & DESC_MANAGER::GetClientSet()
{
	return m_set_pkDesc;
}

struct name_with_desc_func
{
	const char * m_name;

	name_with_desc_func(const char * name) : m_name(name)
	{
	}

	bool operator () (DESC * d)
	{
		if (d->GetCharacter() && !strcmp(d->GetCharacter()->GetName(), m_name))
			return true;

		return false;
	}
};

DESC * DESC_MANAGER::FindByCharacterName(const char *name)
{
	DESC_SET::iterator it = std::find_if (m_set_pkDesc.begin(), m_set_pkDesc.end(), name_with_desc_func(name));
	return (it == m_set_pkDesc.end()) ? nullptr : (*it);
}

CLIENT_DESC * DESC_MANAGER::CreateConnectionDesc(const char * host, uint16_t port, int iPhaseWhenSucceed, bool bRetryWhenClosed)
{
	CLIENT_DESC * newd;

	newd = new CLIENT_DESC;

	newd->Setup(host, port);
	newd->Connect(iPhaseWhenSucceed);
	newd->SetRetryWhenClosed(bRetryWhenClosed);

	m_set_pkDesc.insert(newd);
	m_set_pkClientDesc.insert(newd);

	++m_iSocketsConnected;
	return (newd);
}

struct FuncTryConnect
{
	void operator () (DESC * d)
	{
		((CLIENT_DESC *)d)->Connect();
	}
};

void DESC_MANAGER::TryConnect()
{
	FuncTryConnect f;
	std::for_each(m_set_pkClientDesc.begin(), m_set_pkClientDesc.end(), f);
}

bool DESC_MANAGER::IsP2PDescExist(const char * szHost, uint16_t wPort)
{
	CLIENT_DESC_SET::iterator it = m_set_pkClientDesc.begin();
	
	while (it != m_set_pkClientDesc.end())
	{
		CLIENT_DESC * d = *(it++);

		if (!strcmp(d->GetP2PHost(), szHost) && d->GetP2PPort() == wPort)
			return true;
	}

	return false;
}

class FuncWho
{
	public:
		int iTotalCount;
		int aiEmpireUserCount[EMPIRE_MAX_NUM];

		FuncWho()
		{
			iTotalCount = 0;
			memset(aiEmpireUserCount, 0, sizeof(aiEmpireUserCount));
		}

		void operator() (DESC * d)
		{
			if (d->GetCharacter())
			{
				++iTotalCount;
				++aiEmpireUserCount[d->GetEmpire()];
			}
		}
};

void DESC_MANAGER::UpdateLocalUserCount()
{
	const DESC_SET & c_ref_set = GetClientSet();
	FuncWho f;
	f = std::for_each(c_ref_set.begin(), c_ref_set.end(), f);

	m_iLocalUserCount = f.iTotalCount;
	std::memcpy(m_aiEmpireUserCount, f.aiEmpireUserCount, sizeof(m_aiEmpireUserCount));

	m_aiEmpireUserCount[1] += P2P_MANAGER::instance().GetEmpireUserCount(1);
	m_aiEmpireUserCount[2] += P2P_MANAGER::instance().GetEmpireUserCount(2);
	m_aiEmpireUserCount[3] += P2P_MANAGER::instance().GetEmpireUserCount(3);
}

void DESC_MANAGER::GetUserCount(int & iTotal, int ** paiEmpireUserCount, int & iLocalCount)
{
	*paiEmpireUserCount = &m_aiEmpireUserCount[0];
	
	int iCount = P2P_MANAGER::instance().GetCount();
	if (iCount < 0)
	{
		metin2::log::error("P2P_MANAGER::instance().GetCount() == -1");
	}
	iTotal = m_iLocalUserCount + iCount;
	iLocalCount = m_iLocalUserCount;
}


uint32_t DESC_MANAGER::MakeRandomKey(uint32_t dwHandle)
{ 
	uint32_t random_key = thecore_random();
	m_map_handle_random_key[dwHandle] = random_key;
	return random_key;
}

bool DESC_MANAGER::CheckRandomKey(uint32_t dwHandle, uint32_t dwKey)
{
	auto it = m_map_handle_random_key.find(dwHandle);
	return it != m_map_handle_random_key.end() && it->second == dwKey && FindByHandle(dwHandle) != nullptr;
}

DESC * DESC_MANAGER::FindByLoginKey(uint32_t dwKey)
{
	std::map<uint32_t, CLoginKey *>::iterator it = m_map_pkLoginKey.find(dwKey);

	if (it == m_map_pkLoginKey.end())
		return nullptr;

	return it->second->m_pkDesc;
}


uint32_t DESC_MANAGER::CreateLoginKey(DESC * d)
{
	uint32_t dwKey = 0;

	do
	{
		dwKey = number(1, INT_MAX);

		if (m_map_pkLoginKey.find(dwKey) != m_map_pkLoginKey.end())
			continue;

		CLoginKey * pkKey = new CLoginKey(dwKey, d);
		d->SetLoginKey(pkKey);
		m_map_pkLoginKey.insert(std::make_pair(dwKey, pkKey));
		break;
	} while (1);

	return dwKey;
}

void DESC_MANAGER::ProcessExpiredLoginKey()
{
	uint32_t dwCurrentTime = get_dword_time();

	std::map<uint32_t, CLoginKey *>::iterator it, it2;

	it = m_map_pkLoginKey.begin();

	while (it != m_map_pkLoginKey.end())
	{
		it2 = it++;

		if (it2->second->m_dwExpireTime == 0)
			continue;

		if (dwCurrentTime - it2->second->m_dwExpireTime > 60000)
		{
			delete it2->second;
			m_map_pkLoginKey.erase(it2);
		}
	}
}
}
