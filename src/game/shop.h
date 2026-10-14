#ifndef __INC_METIN_II_GAME_SHOP_H__
#define __INC_METIN_II_GAME_SHOP_H__
#include "typedef.h"

#include <common/tables.h>

#include <cstdint>
#include <unordered_map>
#include <vector>

enum
{
	SHOP_MAX_DISTANCE = 1000
};

class CGrid;

/* ---------------------------------------------------------------------------------- */
class CShop
{
	public:
		typedef struct shop_item
		{
			uint32_t	vnum;		// Item number
			int32_t	price;		// Price
			uint8_t	count;		// Item count

			CItem *	pkItem;
			int		itemid;		// Item unique ID

			shop_item()
			{
				vnum = 0;
				price = 0;
				count = 0;
				itemid = 0;
				pkItem = nullptr;
			}
		} SHOP_ITEM;

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

		CGrid *				m_pGrid;

		typedef std::unordered_map<CHARACTER *, bool> GuestMapType;
		GuestMapType m_map_guest;
		std::vector<SHOP_ITEM>		m_itemVector;	// Goods sold by this shop

		CHARACTER *			m_pkPC;
};

#endif 
