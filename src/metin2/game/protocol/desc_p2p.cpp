#include <metin2/config/config.h>
#include <metin2/game/protocol/desc_p2p.h>

#include <metin2/game/app/config.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/input.h>
#include <metin2/game/protocol/p2p.h>
#include <metin2/game/protocol/packet.h>

#include <metin2/common/peer_security.h>
#include <metin2/log/log.h>
#include <metin2/net/connection.h>

#include <memory>
#include <utility>

namespace metin2::game::inline protocol {

    desc_p2p::~desc_p2p() = default;

    void desc_p2p::destroy() {
        if (!connection_) {
            return;
        }

        p2p_manager::instance().unregister_acceptor(this);

        metin2::log::info("Closing peer connection to {}", host_);

        desc::destroy();
    }

    auto desc_p2p::setup(std::shared_ptr<net::connection> connection) -> bool {
        connection_ = std::move(connection);
        host_ = connection_->remote_address();
        port_ = connection_->remote_port();

        connection_->start(*this);

        if (!is_allowed_peer(host_.c_str(), metin2::config::server().p2p_allowed_hosts, public_ip)) {
            metin2::log::warn("Blocked peer connection from {} to {}: host not allowed", host_, public_ip);
            set_phase(PHASE_CLOSE);
            return true;
        }

        set_phase(PHASE_P2P);

        metin2::log::info("New peer connection from {}", host_);
        return true;
    }

    void desc_p2p::set_phase(int phase) {
        static input_p2p input_p2_p;

        switch (phase) {
        case PHASE_P2P:
            metin2::log::debug("Peer connection entered p2p phase");
            input_processor_ = &input_p2_p;
            break;

        case PHASE_CLOSE:
            input_processor_ = nullptr;
            break;

        default:
            metin2::log::error("Unknown phase {:d} for peer connection", phase);
            break;
        }

        phase_ = phase;
    }
}
