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
		safebox(character * pkChrOwner, int iSize, uint32_t dwGold);
		~safebox();

		bool		add(uint32_t dwPos, item * pkItem);
		item *		get(uint32_t dwPos);
		item *		remove(uint32_t dwPos);
		void		change_size(int iSize);

		bool		move_item(uint8_t bCell, uint8_t bDestCell, uint8_t count);
		item *		get_item(uint8_t bCell);

		void		save();

		bool		is_empty(uint32_t dwPos, uint8_t bSize);
		// Tells the owner's client what is in a slot.
		void		send_item_set(uint32_t dwPos);
		bool		is_valid_position(uint32_t dwPos);

		void		set_window_mode(uint8_t bWindowMode);

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
