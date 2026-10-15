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

void login_data::set_client_key(const uint32_t * client_key)
{
	std::memcpy(&client_key_, client_key, sizeof(uint32_t) * 4);
}

auto login_data::client_key() -> const uint32_t *
{
	return &client_key_[0];
}

void login_data::set_key(uint32_t key)
{
	key_ = key;
}

auto login_data::key() -> uint32_t
{
	return key_;
}

void login_data::set_login(const char * login)
{
	login_ = login;
}

auto login_data::login() -> const char *
{
	return login_.c_str();
}

void login_data::set_premium(int * pai_premium_times)
{
	std::memcpy(premium_times_, pai_premium_times, sizeof(premium_times_));
}

auto login_data::premium_ptr() -> int *
{
	return &premium_times_[0];
}
}
