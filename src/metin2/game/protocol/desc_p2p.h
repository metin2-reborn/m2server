#pragma once

#include <metin2/game/protocol/desc.h>

#include <metin2/net/connection.h>

#include <memory>

namespace metin2::game::inline protocol {

class DESC_P2P : public DESC
{
	public:
		~DESC_P2P() override;

		void	Destroy() override;
		void	SetPhase(int iPhase) override;
		bool		Setup(std::shared_ptr<net::connection> connection);
};
}
