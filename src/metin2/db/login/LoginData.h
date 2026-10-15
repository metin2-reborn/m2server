// vim:ts=8 sw=4
#pragma once

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>

#include <cstdint>

namespace metin2::db::inline login {

class CLoginData
{
    public:
	CLoginData();

	TAccountTable & GetAccountRef();
	void            SetClientKey(const uint32_t * c_pdwClientKey);

	const uint32_t *   GetClientKey();
	void            SetKey(uint32_t dwKey);
	uint32_t           GetKey();

	void            SetConnectedPeerHandle(uint32_t dwHandle);
	uint32_t		GetConnectedPeerHandle();

	void		SetPlay(bool bOn);
	bool		IsPlay();

	void		SetDeleted(bool bSet);
	bool		IsDeleted();

	void            SetPremium(int * paiPremiumTimes);
	int *           GetPremiumPtr();

	uint32_t		GetLastPlayerID() const { return m_dwLastPlayerID; }
	void		SetLastPlayerID(uint32_t id) { m_dwLastPlayerID = id; }

    private:
	uint32_t           m_dwKey;
	uint32_t           m_adwClientKey[4];
	uint32_t           m_dwConnectedPeerHandle;
	bool		m_bPlay;
	bool		m_bDeleted;

	int		m_aiPremiumTimes[PREMIUM_MAX_NUM];

	uint32_t		m_dwLastPlayerID;

	TAccountTable   m_data;
};
}
