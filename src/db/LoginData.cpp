#include "LoginData.h"

#include <common/tables.h>
#include <libthecore/log.h>

#include <cstdint>
#include <cstring>

CLoginData::CLoginData()
{
	m_dwKey = 0;
	memset(m_adwClientKey, 0, sizeof(m_adwClientKey));
	m_dwConnectedPeerHandle = 0;
	m_bPlay = false;
	m_bDeleted = false;
	m_dwLastPlayerID = 0;

	memset(&m_data, 0, sizeof(TAccountTable));
}

TAccountTable & CLoginData::GetAccountRef()
{
	return m_data;
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

void CLoginData::SetConnectedPeerHandle(uint32_t dwHandle)
{
	m_dwConnectedPeerHandle = dwHandle;
}

uint32_t CLoginData::GetConnectedPeerHandle()
{
	return m_dwConnectedPeerHandle;
}

void CLoginData::SetPlay(bool bOn)
{
	sys_log(0, "SetPlay %s %u %s", bOn ? "on" : "off", GetKey(), m_data.login);
	m_bPlay = bOn;
}

bool CLoginData::IsPlay()
{
	return m_bPlay;
}

void CLoginData::SetDeleted(bool bSet)
{
	m_bDeleted = bSet;
}

bool CLoginData::IsDeleted()
{
	return m_bDeleted;
}

void CLoginData::SetPremium(int * paiPremiumTimes)
{
	std::memcpy(m_aiPremiumTimes, paiPremiumTimes, sizeof(m_aiPremiumTimes));
}

int * CLoginData::GetPremiumPtr()
{
	return &m_aiPremiumTimes[0];
}

