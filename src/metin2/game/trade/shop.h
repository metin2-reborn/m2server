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


class shop
{
	public:
		struct shop_item {
			uint32_t	vnum;
			int32_t	price;
			uint8_t	count;

			game::items::item *	item;
			int		itemid;

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

		virtual bool	add_guest(character * ch,uint32_t owner_vid, bool other_empire);
		void	remove_guest(character * ch);

		virtual int	buy(character * ch, uint8_t pos);

		void	broadcast_update_item(uint8_t pos);

		int		number_by_vnum(uint32_t vnum_value);

		virtual bool	is_selling_item(uint32_t item_id);

		uint32_t	vnum() { return vnum_; }

	protected:
		void	broadcast(const void * data, int bytes);

	protected:
		uint32_t				vnum_;

		std::unique_ptr<grid>		grid_;

		typedef std::unordered_map<character *, bool> guest_map_type;
		guest_map_type map_guest_;
		std::vector<shop_item>		item_vector_;

		character *			pc_;
};
}
