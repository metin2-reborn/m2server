#pragma once

#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/input.h>
#include <metin2/game/app/typedef.h>

#include <cstdint>
#include <ctime>

namespace metin2::game::inline protocol {

class CLIENT_DESC : public DESC
{
	public:
		CLIENT_DESC();
		~CLIENT_DESC() override;

		uint8_t	GetType() override { return DESC_TYPE_CONNECTOR; }
		void	Destroy() override;
		void	SetPhase(int phase) override;

		bool 		Connect(int iPhaseWhenSucceed = 0);
		void		Setup(const char * _host, uint16_t _port);

		void		SetRetryWhenClosed(bool);

		void		DBPacketHeader(uint8_t bHeader, uint32_t dwHandle, uint32_t dwSize);
		void		DBPacket(uint8_t bHeader, uint32_t dwHandle, const void * c_pvData, uint32_t dwSize);
		void		Packet(const void * c_pvData, int iSize);
		bool		IsRetryWhenClosed();

		void		Update(uint32_t t);
		void		UpdateChannelStatus(uint32_t t, bool fForce);

		// Non-destructive close for reuse
		void Reset();

	protected:
		int			m_iPhaseWhenSucceed;
		bool		m_bRetryWhenClosed;
		time_t		m_LastTryToConnectTime;
		time_t		m_tLastChannelStatusUpdateTime;

		CInputDB 	m_inputDB;
		CInputP2P 	m_inputP2P;
};

extern CLIENT_DESC * db_clientdesc;
extern CLIENT_DESC * g_pkAuthMasterDesc;
extern CLIENT_DESC * g_NetmarbleDBDesc;
}
