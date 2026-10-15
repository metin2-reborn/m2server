#pragma once
#include <metin2/game/app/typedef.h>

#include <metin2/proto/length.h>
#include <metin2/common/grid.h>

#include <cstdint>
#include <memory>

namespace metin2::game::inline trade {


enum exchange_values
{
	EXCHANGE_ITEM_MAX_NUM 	= 12,
	EXCHANGE_MAX_DISTANCE	= 1000
};

class exchange
{
	public:
		exchange(character * owner);
		~exchange();

		bool		accept(bool accept = true);
		void		cancel();

		bool		add_gold(int32_t gold);
		bool		add_item(item_pos item_pos, uint8_t display_pos);
		bool		remove_item(uint8_t pos);

		character *	owner()	{ return owner_;	}
		exchange *	company()	{ return company_;	}

		bool		accept_status() { return accept_; }

		void		set_company(exchange * exchange_value)	{ company_ = exchange_value; }

	private:
		bool		done();
		bool		check(int * item_count_value);
		bool		check_space();

	private:
		exchange *	company_;	// The other party's CExchange pointer

		character *	owner_;

		item_pos		item_pos_[EXCHANGE_ITEM_MAX_NUM];
		item *		ap_items_[EXCHANGE_ITEM_MAX_NUM];
		uint8_t		item_display_pos_[EXCHANGE_ITEM_MAX_NUM];

		bool 		accept_;
		int32_t		gold_;

		std::unique_ptr<grid>	grid_;

};
}
