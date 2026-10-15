#include <metin2/game/protocol/login_data.h>

#include <cstdint>
#include <cstring>
#include <string>

namespace metin2::game::inline protocol {


login_data::login_data()
{
	key_ = 0;
	memset(client_key_, 0, sizeof(client_key_));
	memset(premium_times_, 0, sizeof(premium_times_));
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

void login_data::set_login(const char * c_pszLogin)
{
	login_ = c_pszLogin;
}

const char * login_data::login()
{
	return login_.c_str();
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
