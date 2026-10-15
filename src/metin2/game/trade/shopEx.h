#pragma once
#include <metin2/game/trade/shop.h>
#include <metin2/game/app/typedef.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace metin2::game::inline data {
    class CGroupNode;
}

namespace metin2::game::inline trade {

struct TShopTableEx : TShopTable {
	std::string name;
	EShopCoinType coinType;
};


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
}
