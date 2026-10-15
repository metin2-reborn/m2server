#include <metin2/game/protocol/desc_p2p.h>

#include <metin2/game/app/config.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/input.h>
#include <metin2/game/protocol/p2p.h>
#include <metin2/game/protocol/packet.h>

#include <metin2/common/peer_security.h>
#include <metin2/net/connection.h>
#include <metin2/log/log.h>

#include <memory>
#include <utility>

namespace metin2::game::inline protocol {


desc_p2_p::~desc_p2_p()
{
}

void desc_p2_p::destroy()
{
	if (!connection_) {
		return;
	}

	p2_p_manager::instance().unregister_acceptor(this);

	metin2::log::info( "SYSTEM: closing p2p connection of {}", host_.c_str());

	// Chain up to base class Destroy()
	desc::destroy();
}

bool desc_p2_p::setup(std::shared_ptr<net::connection> connection)
{
	connection_ = std::move(connection);
	host_ = connection_->remote_address();
	port_ = connection_->remote_port();

	connection_->start(*this);

	if (!is_allowed_peer(host_.c_str(), metin2::config::server().p2p_allowed_hosts, public_ip)) // refuse if remote host != public ip (only the same machine must be able to connect in here)
	{
		metin2::log::info( "PORT_SECURITY: new p2p connection from [{}] to [{}] BLOCKED", host_.c_str(), public_ip);
		set_phase(PHASE_CLOSE);
		return true;
	}

	set_phase(PHASE_P2P);

	metin2::log::info( "SYSTEM: new p2p connection from [{}]", host_.c_str());
	return true;
}

void desc_p2_p::set_phase(int iPhase)
{
	static input_p2_p s_inputP2P;

	switch (iPhase)
	{
		case PHASE_P2P:
			metin2::log::debug( "PHASE_P2P");
			input_processor_ = &s_inputP2P;
			break;

		case PHASE_CLOSE:
			input_processor_ = nullptr;
			break;

		default:
			metin2::log::error("DESC_P2P::SetPhase : Unknown phase");
			break;
	}

	phase_ = iPhase;
}
}
