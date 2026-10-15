// vim:ts=8 sw=4
#pragma once

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>

#include <cstdint>

namespace metin2::db::inline login {

class login_data
{
    public:
	login_data();

	account_table & account_ref();
	void            set_client_key(const uint32_t * c_pdwClientKey);

	const uint32_t *   client_key();
	void            set_key(uint32_t dwKey);
	uint32_t           key();

	void            set_connected_peer_handle(uint32_t dwHandle);
	uint32_t		connected_peer_handle();

	void		set_play(bool bOn);
	bool		is_play();

	void		set_deleted(bool bSet);
	bool		is_deleted();

	void            set_premium(int * paiPremiumTimes);
	int *           premium_ptr();

	uint32_t		last_player_id() const { return last_player_id_; }
	void		set_last_player_id(uint32_t id) { last_player_id_ = id; }

    private:
	uint32_t           key_;
	uint32_t           client_key_[4];
	uint32_t           connected_peer_handle_;
	bool		play_;
	bool		deleted_;

	int		premium_times_[PREMIUM_MAX_NUM];

	uint32_t		last_player_id_;

	account_table   data_;
};
}
