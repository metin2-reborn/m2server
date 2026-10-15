#include <metin2/db/login/login_data.h>

#include <metin2/proto/tables.h>
#include <metin2/log/log.h>

#include <cstdint>
#include <cstring>

namespace metin2::db::inline login {

login_data::login_data()
{
	key_ = 0;
	memset(client_key_, 0, sizeof(client_key_));
	connected_peer_handle_ = 0;
	play_ = false;
	deleted_ = false;
	last_player_id_ = 0;

	memset(&data_, 0, sizeof(account_table));
}

account_table & login_data::account_ref()
{
	return data_;
}

void login_data::set_client_key(const uint32_t * c_pdwClientKey)
{
	std::memcpy(&client_key_, c_pdwClientKey, sizeof(uint32_t) * 4);
}

const uint32_t * login_data::client_key()
{
	return &client_key_[0];
}

void login_data::set_key(uint32_t dwKey)
{
	key_ = dwKey;
}

uint32_t login_data::key()
{
	return key_;
}

void login_data::set_connected_peer_handle(uint32_t dwHandle)
{
	connected_peer_handle_ = dwHandle;
}

uint32_t login_data::connected_peer_handle()
{
	return connected_peer_handle_;
}

void login_data::set_play(bool bOn)
{
	metin2::log::info( "SetPlay {} {:d} {}", bOn ? "on" : "off", key(), data_.login);
	play_ = bOn;
}

bool login_data::is_play()
{
	return play_;
}

void login_data::set_deleted(bool bSet)
{
	deleted_ = bSet;
}

bool login_data::is_deleted()
{
	return deleted_;
}

void login_data::set_premium(int * paiPremiumTimes)
{
	std::memcpy(premium_times_, paiPremiumTimes, sizeof(premium_times_));
}

int * login_data::premium_ptr()
{
	return &premium_times_[0];
}
}
