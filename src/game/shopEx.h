#ifndef __INC_METIN_II_GAME_SHOP_SECONDARY_COIN_H__
#define __INC_METIN_II_GAME_SHOP_SECONDARY_COIN_H__
#include "shop.h"
#include "typedef.h"

#include <common/length.h>
#include <common/tables.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct SShopTable;
typedef struct SShopTableEx : SShopTable
{
	std::string name;
	EShopCoinType coinType;
} TShopTableEx;

class CGroupNode;

// Extended shop.
// Can use Myeongdojeon coins as currency, and items can be split across multiple tabs.
// However, pc shops are not implemented.
// When communicating with the client, tabs are separated in units of pos 45.
// The old shop's m_itemVector is not used.
class CShopEx: public CShop
{
public:
	bool			Create(uint32_t dwVnum, uint32_t dwNPCVnum);
	bool			AddShopTable(TShopTableEx& shopTable);

	bool	AddGuest(CHARACTER * ch,uint32_t owner_vid, bool bOtherEmpire) override;
	void	SetPCShop(CHARACTER * ch) override { return; }
	bool	IsPCShop() override { return false; }
	int		Buy(CHARACTER * ch, uint8_t pos) override;
	bool	IsSellingItem(uint32_t itemID) override { return false; }
	
	size_t			GetTabCount() { return m_vec_shopTabs.size(); }
private:
	std::vector <TShopTableEx> m_vec_shopTabs;
};

#endif
