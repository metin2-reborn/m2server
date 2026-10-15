#pragma once
#include <metin2/proto/length.h>

#include <cstdint>
#include <string>

namespace metin2::game::inline protocol {

class CLoginData
{
	public:
		CLoginData();

		void            SetClientKey(const uint32_t * c_pdwClientKey);

		const uint32_t *   GetClientKey();
		void            SetKey(uint32_t dwKey);
		uint32_t           GetKey();

		void		SetLogin(const char * c_pszLogin);
		const char *	GetLogin();

		void		SetPremium(int * paiPremiumTimes);
		int *		GetPremiumPtr();

	private:
		uint32_t           m_dwKey;
		uint32_t           m_adwClientKey[4];
		std::string	m_stLogin;
		int		m_aiPremiumTimes[PREMIUM_MAX_NUM];
};
}
