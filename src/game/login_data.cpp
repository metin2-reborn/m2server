#include "login_data.h"

#include <cstdint>
#include <cstring>
#include <string>


CLoginData::CLoginData()
{
	m_dwKey = 0;
	memset(m_adwClientKey, 0, sizeof(m_adwClientKey));
	memset(m_aiPremiumTimes, 0, sizeof(m_aiPremiumTimes));
}

void CLoginData::SetClientKey(const uint32_t * c_pdwClientKey)
{
	std::memcpy(&m_adwClientKey, c_pdwClientKey, sizeof(uint32_t) * 4);
}

const uint32_t * CLoginData::GetClientKey()
{
	return &m_adwClientKey[0];
}

void CLoginData::SetKey(uint32_t dwKey)
{
	m_dwKey = dwKey;
}

uint32_t CLoginData::GetKey()
{
	return m_dwKey;
}

void CLoginData::SetLogin(const char * c_pszLogin)
{
	m_stLogin = c_pszLogin;
}

const char * CLoginData::GetLogin()
{
	return m_stLogin.c_str();
}

void CLoginData::SetPremium(int * paiPremiumTimes)
{
	std::memcpy(m_aiPremiumTimes, paiPremiumTimes, sizeof(m_aiPremiumTimes));
}

int * CLoginData::GetPremiumPtr()
{
	return &m_aiPremiumTimes[0];
}

