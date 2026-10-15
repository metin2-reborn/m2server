#pragma once
#include <metin2/proto/length.h>

#include <cstdint>
#include <string>

namespace metin2::game::inline protocol {

class login_data
{
	public:
		login_data();

		void            set_client_key(const uint32_t * c_pdwClientKey);

		const uint32_t *   client_key();
		void            set_key(uint32_t dwKey);
		uint32_t           key();

		void		set_login(const char * c_pszLogin);
		const char *	login();

		void		set_premium(int * paiPremiumTimes);
		int *		premium_ptr();

	private:
		uint32_t           key_;
		uint32_t           client_key_[4];
		std::string	login_;
		int		premium_times_[PREMIUM_MAX_NUM];
};
}
