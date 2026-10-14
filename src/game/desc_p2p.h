#pragma once

#include "desc.h"

#include <libthecore/fdwatch.h>
#include <libthecore/typedef.h>

#include <cstdint>

class DESC_P2P : public DESC
{
	public:
		~DESC_P2P() override;

		void	Destroy() override;
		void	SetPhase(int iPhase) override;
		bool		Setup(fdwatch * _fdw, socket_t fd, const char * host, uint16_t wPort);
};

