#pragma once
#include "typedef.h"

#include <common/tables.h>
#include <libgame/grid.h>

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace metin2::game {

enum
{
	SHOP_MAX_DISTANCE = 1000
};


/* ---------------------------------------------------------------------------------- */
class CShop
{
	public:
		struct SHOP_ITEM {
			uint32_t	vnum;		// Item number
			int32_t	price;		// Price
			uint8_t	count;		// Item count

			CItem *	pkItem;
			int		itemid;		// Item unique ID

			SHOP_ITEM()
			{
				vnum = 0;
				price = 0;
				count = 0;
				itemid = 0;
				pkItem = nullptr;
			}
		};

		CShop();
		virtual ~CShop();

		bool	Create(uint32_t dwVnum, uint32_t dwNPCVnum, TShopItemTable * pItemTable);
		void	SetShopItems(TShopItemTable * pItemTable, uint8_t bItemCount);

		virtual void	SetPCShop(CHARACTER * ch);
		virtual bool	IsPCShop()	{ return m_pkPC ? true : false; }

		// Add/remove guest
		virtual bool	AddGuest(CHARACTER * ch,uint32_t owner_vid, bool bOtherEmpire);
		void	RemoveGuest(CHARACTER * ch);

		// Buy goods
		virtual int	Buy(CHARACTER * ch, uint8_t pos);

		// Sends a packet to guests
		void	BroadcastUpdateItem(uint8_t pos);

		// Returns the number of items on sale.
		int		GetNumberByVnum(uint32_t dwVnum);

		// Tells whether the item is listed in the shop.
		virtual bool	IsSellingItem(uint32_t itemID);

		uint32_t	GetVnum() { return m_dwVnum; }

	protected:
		void	Broadcast(const void * data, int bytes);

	protected:
		uint32_t				m_dwVnum;

		std::unique_ptr<CGrid>		m_pGrid;

		typedef std::unordered_map<CHARACTER *, bool> GuestMapType;
		GuestMapType m_map_guest;
		std::vector<SHOP_ITEM>		m_itemVector;	// Goods sold by this shop

		CHARACTER *			m_pkPC;
};
}
