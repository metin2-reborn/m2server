#include "desc_client.h"

#include "buffer_manager.h"
#include "config.h"
#include "desc.h"
#include "desc_manager.h"
#include "guild_manager.h"
#include "p2p.h"
#include "packet.h"
#include "party.h"
#include "protocol.h"
#include "typedef.h"
#include "utils.h"

#include <common/tables.h>
#include <libnet/connection.h>
#include <libnet/io.h>
#include <metin2/log/log.h>
#include <libthecore/main.h>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>

namespace metin2::game {


CLIENT_DESC * db_clientdesc = nullptr;
CLIENT_DESC * g_pkAuthMasterDesc = nullptr;
CLIENT_DESC * g_NetmarbleDBDesc = nullptr;

static const char* GetKnownClientDescName(CLIENT_DESC * desc) {
	if (desc == db_clientdesc) {
		return "db_clientdesc";
	} else if (desc == g_pkAuthMasterDesc) {
		return "g_pkAuthMasterDesc";
	} else if (desc == g_NetmarbleDBDesc) {
		return "g_NetmarbleDBDesc";
	}
	return "unknown";
}

CLIENT_DESC::CLIENT_DESC()
{
	m_iPhaseWhenSucceed = 0;
	m_bRetryWhenClosed = false;
	m_LastTryToConnectTime = 0;
	m_tLastChannelStatusUpdateTime = 0;
}

CLIENT_DESC::~CLIENT_DESC()
{
}

void CLIENT_DESC::Destroy()
{
	if (!m_connection) {
		return;
	}

	P2P_MANAGER::instance().UnregisterConnector(this);

	if (this == db_clientdesc)
	{
		CPartyManager::instance().DeleteAllParty();
		CPartyManager::instance().DisablePCParty();
		CGuildManager::instance().StopAllGuildWar();
	}

	metin2::log::info( "SYSTEM: closing connection to {}:{:d}", m_stHost.c_str(), m_wPort);

	// Chain up to base class Destroy()
	DESC::Destroy();
}

void CLIENT_DESC::SetRetryWhenClosed(bool b)
{
	m_bRetryWhenClosed = b;
}

bool CLIENT_DESC::Connect(int iPhaseWhenSucceed)
{
	if (iPhaseWhenSucceed != 0)
		m_iPhaseWhenSucceed = iPhaseWhenSucceed;

	if (get_global_time() - m_LastTryToConnectTime < 3)	// 3 seconds
		return false;

	m_LastTryToConnectTime = get_global_time();

	if (m_connection)
		return false;

	metin2::log::info( "SYSTEM: Trying to connect to {}:{:d}", m_stHost.c_str(), m_wPort);

	// Blocking (at most 10 seconds) so that packets sent right after the connect go out in order.
	auto socket = net::connect(m_stHost, m_wPort, std::chrono::seconds(10));

	if (socket)
	{
		m_connection = net::connection::create(std::move(*socket), MAX_SERVER_INPUT, MAX_SERVER_OUTPUT);
		m_connection->start(*this);
		metin2::log::info( "SYSTEM: connected to server (ptr {})", static_cast<const void *>(this));
		SetPhase(m_iPhaseWhenSucceed);
		return true;
	}
	else
	{
		SetPhase(PHASE_CLIENT_CONNECTING);
		return false;
	}
}

void CLIENT_DESC::Setup(const char * _host, uint16_t _port)
{
	m_stHost = _host;
	m_wPort = _port;
}

void CLIENT_DESC::SetPhase(int iPhase)
{
	switch (iPhase)
	{
		case PHASE_CLIENT_CONNECTING:
			metin2::log::debug( "PHASE_CLIENT_DESC::CONNECTING");
			m_pInputProcessor = nullptr;
			break;

		case PHASE_DBCLIENT:
			{
				metin2::log::debug( "PHASE_DBCLIENT");

				if (!metin2::config::is_auth())
				{
					static bool bSentBoot = false;

					if (!bSentBoot)
					{
						bSentBoot = true;
						TPacketGDBoot p;
						p.dwItemIDRange[0] = 0;
						p.dwItemIDRange[1] = 0;
						memcpy(p.szIP, g_szPublicIP, 16);
						DBPacket(HEADER_GD_BOOT, 0, &p, sizeof(p));
					}
				}

				TEMP_BUFFER buf;

				TPacketGDSetup p;

				memset(&p, 0, sizeof(p));
				strlcpy(p.szPublicIP, g_szPublicIP, sizeof(p.szPublicIP));

				if (!metin2::config::is_auth())
				{
					p.bChannel	= metin2::config::server().channel;
					p.wListenPort = metin2::config::server().port;
					p.wP2PPort	= metin2::config::server().p2p_port;
					p.bAuthServer = false;
					map_allow_copy(p.alMaps, MAX_MAPS_PER_SERVER);

					const DESC_MANAGER::DESC_SET & c_set = DESC_MANAGER::instance().GetClientSet();
					DESC_MANAGER::DESC_SET::const_iterator it;

					for (it = c_set.begin(); it != c_set.end(); ++it)
					{
						DESC * d = *it;

						if (d->GetAccountTable().id != 0)
							++p.dwLoginCount;
					}

					buf.write(&p, sizeof(p));

					if (p.dwLoginCount)
					{
						TPacketLoginOnSetup pck;

						for (it = c_set.begin(); it != c_set.end(); ++it)
						{
							DESC * d = *it;

							TAccountTable & r = d->GetAccountTable();

							if (r.id != 0)
							{
								pck.dwID = r.id;
								strlcpy(pck.szLogin, r.login, sizeof(pck.szLogin));
								strlcpy(pck.szSocialID, r.social_id, sizeof(pck.szSocialID));
								strlcpy(pck.szHost, d->GetHostName(), sizeof(pck.szHost));
								pck.dwLoginKey = d->GetLoginKey();

								buf.write(&pck, sizeof(TPacketLoginOnSetup));
							}
						}
					}

					metin2::log::info( "DB_SETUP current user {:d} size {:d}", p.dwLoginCount, buf.size());

					// Party processing is now possible.
					CPartyManager::instance().EnablePCParty();
				}
				else
				{
					p.bAuthServer = true;
					buf.write(&p, sizeof(p));
				}

				DBPacket(HEADER_GD_SETUP, 0, buf.read_peek(), buf.size());
				m_pInputProcessor = &m_inputDB;
			}
			break;

		case PHASE_P2P:
			metin2::log::debug( "PHASE_P2P");
			m_pInputProcessor = &m_inputP2P;
			break;

		case PHASE_CLOSE:
			m_pInputProcessor = nullptr;
			break;

	}

	m_iPhase = iPhase;
}

void CLIENT_DESC::DBPacketHeader(uint8_t bHeader, uint32_t dwHandle, uint32_t dwSize)
{
	m_connection->send(encode_byte(bHeader), sizeof(uint8_t));
	m_connection->send(encode_4bytes(dwHandle), sizeof(uint32_t));
	m_connection->send(encode_4bytes(dwSize), sizeof(uint32_t));
}

void CLIENT_DESC::DBPacket(uint8_t bHeader, uint32_t dwHandle, const void * c_pvData, uint32_t dwSize)
{
	if (!m_connection) {
		metin2::log::info( "CLIENT_DESC [{}] trying DBPacket() while not connected",
			GetKnownClientDescName(this));
		return;
	}
	metin2::log::debug( "DB_PACKET: header {:d} handle {:d} size {:d} pending {:d}", bHeader, dwHandle, dwSize, m_connection->pending_output());
	DBPacketHeader(bHeader, dwHandle, dwSize);

	if (c_pvData)
		m_connection->send(c_pvData, dwSize);
}

void CLIENT_DESC::Packet(const void * c_pvData, int iSize)
{
	if (!m_connection) {
		metin2::log::info( "CLIENT_DESC [{}] trying Packet() while not connected",
			GetKnownClientDescName(this));
		return;
	}
	m_connection->send(c_pvData, iSize);
}

bool CLIENT_DESC::IsRetryWhenClosed()
{
	return (0 == thecore_is_shutdowned() && m_bRetryWhenClosed);
}

void CLIENT_DESC::Update(uint32_t t)
{
	if (!metin2::config::is_auth()) {
		UpdateChannelStatus(t, false);
	}
}

void CLIENT_DESC::UpdateChannelStatus(uint32_t t, bool fForce)
{
	enum {
		CHANNELSTATUS_UPDATE_PERIOD = 5*60*1000,	// Every 5 minutes
	};
	if (fForce || m_tLastChannelStatusUpdateTime+CHANNELSTATUS_UPDATE_PERIOD < t) {
		int iTotal; 
		int * paiEmpireUserCount;
		int iLocal;
		DESC_MANAGER::instance().GetUserCount(iTotal, &paiEmpireUserCount, iLocal);

		TChannelStatus channelStatus;
		channelStatus.nPort = metin2::config::server().port;

		if (g_bNoMoreClient) channelStatus.bStatus = 0;
		else channelStatus.bStatus = iTotal > g_iFullUserCount ? 3 : iTotal > g_iBusyUserCount ? 2 : 1;

		DBPacket(HEADER_GD_UPDATE_CHANNELSTATUS, 0, &channelStatus, sizeof(channelStatus));
		m_tLastChannelStatusUpdateTime = t;
	}
}

void CLIENT_DESC::Reset()
{
	// Backup connection target info
	std::string host = m_stHost;
	uint16_t port = m_wPort;

	Destroy();
	Initialize();

	// Restore connection target info
	m_stHost = host;
	m_wPort = port;
}
}
