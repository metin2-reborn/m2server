#pragma once
#include <metin2/proto/length.h>

#include <cstdint>
#include <string>

namespace metin2::game::inline protocol {

class login_data
{
	public:
		login_data();

		void            set_client_key(const uint32_t * client_key);

		auto   client_key() -> const uint32_t *;
		void            set_key(uint32_t key);
		auto           key() -> uint32_t;

		void		set_login(const char * login);
		auto	login() -> const char *;

		void		set_premium(int * pai_premium_times);
		auto		premium_ptr() -> int *;

	private:
		uint32_t           key_;
		uint32_t           client_key_[4];
		std::string	login_;
		int		premium_times_[PREMIUM_MAX_NUM];
};
}
