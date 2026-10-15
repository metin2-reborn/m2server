#pragma once
#include <metin2/game/app/typedef.h>

#include <metin2/proto/tables.h>
#include <metin2/common/grid.h>

#include <cstdint>
#include <memory>

namespace metin2::game::inline characters {
    class character;
}
namespace metin2::game::inline items {
    class item;
}

namespace metin2::game::inline trade {


class safebox
{
	public:
		safebox(character * chr_owner, int size, uint32_t gold);
		~safebox();

		bool		add(uint32_t pos, item * item_value);
		item *		get(uint32_t pos);
		item *		remove(uint32_t pos_value);
		void		change_size(int size);

		bool		move_item(uint8_t cell, uint8_t dest_cell, uint8_t count);
		item *		get_item(uint8_t cell);

		void		save();

		bool		is_empty(uint32_t pos, uint8_t size);
		// Tells the owner's client what is in a slot.
		void		send_item_set(uint32_t pos);
		bool		is_valid_position(uint32_t pos);

		void		set_window_mode(uint8_t mode);

	protected:
		void		destroy();

		character *	chr_owner_;
		item *		items_[SAFEBOX_MAX_NUM];
		std::unique_ptr<grid>	grid_;
		int		size_;
		int32_t		gold_;

		uint8_t		window_mode_;
};
}
