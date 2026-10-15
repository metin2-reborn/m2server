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
    class group_node;
}

namespace metin2::game::inline trade {

struct shop_table_ex : shop_table {
	std::string name;
	shop_coin_type coin_type;
};


// Extended shop.
// Can use Myeongdojeon coins as currency, and items can be split across multiple tabs.
// However, pc shops are not implemented.
// When communicating with the client, tabs are separated in units of pos 45.
// The old shop's m_itemVector is not used.
class shop_ex: public shop
{
public:
	bool			create(uint32_t vnum, uint32_t npc_vnum);
	bool			add_shop_table(shop_table_ex& shop_table);

	bool	add_guest(character * ch,uint32_t owner_vid, bool other_empire) override;
	void	set_pc_shop(character * ch) override { return; }
	bool	is_pc_shop() override { return false; }
	int		buy(character * ch, uint8_t pos) override;
	bool	is_selling_item(uint32_t item_id) override { return false; }
	
	size_t			tab_count() { return vec_shop_tabs_.size(); }
private:
	std::vector <shop_table_ex> vec_shop_tabs_;
};
}
