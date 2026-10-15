#pragma once

#include <metin2/game/protocol/desc.h>

#include <metin2/net/connection.h>

#include <memory>

namespace metin2::game::inline protocol {

class desc_p2_p : public desc
{
	public:
		~desc_p2_p() override;

		void	destroy() override;
		void	set_phase(int iPhase) override;
		bool		setup(std::shared_ptr<net::connection> connection);
};
}
