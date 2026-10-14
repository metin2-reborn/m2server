#pragma once

#include "desc.h"

#include <libnet/connection.h>

#include <memory>

class DESC_P2P : public DESC
{
	public:
		~DESC_P2P() override;

		void	Destroy() override;
		void	SetPhase(int iPhase) override;
		bool		Setup(std::shared_ptr<net::connection> connection);
};

