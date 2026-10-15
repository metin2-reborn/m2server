#pragma once
#include <metin2/game/app/typedef.h>

#include <metin2/proto/tables.h>
#include <metin2/common/grid.h>

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace metin2::game::inline trade {

enum
{
	SHOP_MAX_DISTANCE = 1000
};


/* ---------------------------------------------------------------------------------- */
class shop
{
	public:
		struct shop_item {
			uint32_t	vnum;		// Item number
			int32_t	price;		// Price
			uint8_t	count;		// Item count

			game::items::item *	item;
			int		itemid;		// Item unique ID

			shop_item()
			{
				vnum = 0;
				price = 0;
				count = 0;
				itemid = 0;
				item = nullptr;
			}
		};

		shop();
		virtual ~shop();

		bool	create(uint32_t vnum_value, uint32_t npc_vnum, shop_item_table * table);
		void	set_shop_items(shop_item_table * table_value, uint8_t item_count);

		virtual void	set_pc_shop(character * ch);
		virtual bool	is_pc_shop()	{ return pc_ ? true : false; }

		// Add/remove guest
		virtual bool	add_guest(character * ch,uint32_t owner_vid, bool other_empire);
		void	remove_guest(character * ch);

		// Buy goods
		virtual int	buy(character * ch, uint8_t pos);

		// Sends a packet to guests
		void	broadcast_update_item(uint8_t pos);

		// Returns the number of items on sale.
		int		number_by_vnum(uint32_t vnum_value);

		// Tells whether the item is listed in the shop.
		virtual bool	is_selling_item(uint32_t item_id);

		uint32_t	vnum() { return vnum_; }

	protected:
		void	broadcast(const void * data, int bytes);

	protected:
		uint32_t				vnum_;

		std::unique_ptr<grid>		grid_;

		typedef std::unordered_map<character *, bool> guest_map_type;
		guest_map_type map_guest_;
		std::vector<shop_item>		item_vector_;	// Goods sold by this shop

		character *			pc_;
};
}
