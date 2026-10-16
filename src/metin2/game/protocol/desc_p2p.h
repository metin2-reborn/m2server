#pragma once

#include <metin2/game/protocol/desc.h>

#include <metin2/net/connection.h>

#include <memory>

namespace metin2::game::inline protocol {

    class desc_p2p : public desc {
      public:
        ~desc_p2p() override;

        void destroy() override;
        void set_phase(int phase) override;
        auto setup(std::shared_ptr<net::connection> connection) -> bool;
    };
}
