#include "desc_p2p.h"

#include "config.h"
#include "desc.h"
#include "input.h"
#include "p2p.h"
#include "packet.h"

#include <common/peer_security.h>
#include <libnet/connection.h>
#include <metin2/log/log.h>

#include <memory>
#include <utility>


DESC_P2P::~DESC_P2P()
{
}

void DESC_P2P::Destroy()
{
	if (!m_connection) {
		return;
	}

	P2P_MANAGER::instance().UnregisterAcceptor(this);

	metin2::log::info( "SYSTEM: closing p2p connection of {}", m_stHost.c_str());

	// Chain up to base class Destroy()
	DESC::Destroy();
}

bool DESC_P2P::Setup(std::shared_ptr<net::connection> connection)
{
	m_connection = std::move(connection);
	m_stHost = m_connection->remote_address();
	m_wPort = m_connection->remote_port();

	m_connection->start(*this);

	if (!is_allowed_peer(m_stHost.c_str(), g_allowedPeerHosts, g_szPublicIP)) // refuse if remote host != public ip (only the same machine must be able to connect in here)
	{
		metin2::log::info( "PORT_SECURITY: new p2p connection from [{}] to [{}] BLOCKED", m_stHost.c_str(), g_szPublicIP);
		SetPhase(PHASE_CLOSE);
		return true;
	}

	SetPhase(PHASE_P2P);

	metin2::log::info( "SYSTEM: new p2p connection from [{}]", m_stHost.c_str());
	return true;
}

void DESC_P2P::SetPhase(int iPhase)
{
	static CInputP2P s_inputP2P;

	switch (iPhase)
	{
		case PHASE_P2P:
			metin2::log::debug( "PHASE_P2P");
			m_pInputProcessor = &s_inputP2P;
			break;

		case PHASE_CLOSE:
			m_pInputProcessor = nullptr;
			break;

		default:
			metin2::log::error("DESC_P2P::SetPhase : Unknown phase");
			break;
	}

	m_iPhase = iPhase;
}

