#pragma once
#include "typedef.h"

#include <common/length.h>

#include <cstdint>
#include <memory>

class CGrid;

enum EExchangeValues
{
	EXCHANGE_ITEM_MAX_NUM 	= 12,
	EXCHANGE_MAX_DISTANCE	= 1000
};

class CExchange
{
	public:
		CExchange(CHARACTER * pOwner);
		~CExchange();

		bool		Accept(bool bIsAccept = true);
		void		Cancel();

		bool		AddGold(int32_t lGold);
		bool		AddItem(TItemPos item_pos, uint8_t display_pos);
		bool		RemoveItem(uint8_t pos);

		CHARACTER *	GetOwner()	{ return m_pOwner;	}
		CExchange *	GetCompany()	{ return m_pCompany;	}

		bool		GetAcceptStatus() { return m_bAccept; }

		void		SetCompany(CExchange * pExchange)	{ m_pCompany = pExchange; }

	private:
		bool		Done();
		bool		Check(int * piItemCount);
		bool		CheckSpace();

	private:
		CExchange *	m_pCompany;	// The other party's CExchange pointer

		CHARACTER *	m_pOwner;

		TItemPos		m_aItemPos[EXCHANGE_ITEM_MAX_NUM];
		CItem *		m_apItems[EXCHANGE_ITEM_MAX_NUM];
		uint8_t		m_abItemDisplayPos[EXCHANGE_ITEM_MAX_NUM];

		bool 		m_bAccept;
		int32_t		m_lGold;

		std::unique_ptr<CGrid>	m_pGrid;

};
