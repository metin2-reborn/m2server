#pragma once

#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/input.h>
#include <metin2/game/app/typedef.h>

#include <cstdint>
#include <ctime>

namespace metin2::game::inline protocol {

class client_desc : public desc
{
	public:
		client_desc();
		~client_desc() override;

		uint8_t	type() override { return DESC_TYPE_CONNECTOR; }
		void	destroy() override;
		void	set_phase(int phase) override;

		bool 		connect(int iPhaseWhenSucceed = 0);
		void		setup(const char * _host, uint16_t _port);

		void		set_retry_when_closed(bool);

		void		db_packet_header(uint8_t bHeader, uint32_t dwHandle, uint32_t dwSize);
		void		db_packet(uint8_t bHeader, uint32_t dwHandle, const void * c_pvData, uint32_t dwSize);
		void		packet(const void * c_pvData, int iSize);
		bool		is_retry_when_closed();

		void		update(uint32_t t);
		void		update_channel_status(uint32_t t, bool fForce);

		// Non-destructive close for reuse
		void reset();

	protected:
		int			phase_when_succeed_;
		bool		retry_when_closed_;
		time_t		last_try_to_connect_time_;
		time_t		t_last_channel_status_update_time_;

		input_db 	input_db_;
		input_p2_p 	input_p2_p_;
};

extern client_desc * db_clientdesc;
extern client_desc * auth_master_desc;
extern client_desc * netmarble_db_desc;
}
