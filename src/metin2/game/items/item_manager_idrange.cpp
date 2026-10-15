
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/items/item_manager.h>

#include <metin2/proto/tables.h>
#include <metin2/log/log.h>
#include <metin2/core/main.h>

#include <cassert>
#include <cstdint>
#include <cstdio>

namespace metin2::game::inline items {

int touch(const char *path)
{
	FILE	*fp;

	if ( !(fp = fopen(path, "a")) )
	{
		metin2::log::error("touch failed");
		return (-1);
	}

	fclose(fp);
	return 0;
}

uint32_t item_manager::new_id()
{
	assert(current_id_ != 0);

	if ( current_id_ >= item_id_range_.max )
	{
		if ( item_id_spare_range_.min == 0 || item_id_spare_range_.max == 0 || item_id_spare_range_.usable_item_id_min == 0 )
		{
			metin2::log::error("ItemIDRange: FATAL ERROR!!! no more item id");
			touch(".killscript");
			thecore_shutdown();
			return 0;
		}
		else
		{
			metin2::log::info( "ItemIDRange: First Range is full. Change to SpareRange {:d} ~ {:d} {:d}",
					item_id_spare_range_.min, item_id_spare_range_.max, item_id_spare_range_.usable_item_id_min);

			db_clientdesc->db_packet(HEADER_GD_REQ_SPARE_ITEM_ID_RANGE, 0, &item_id_range_.max, sizeof(uint32_t));

			set_max_item_id(item_id_spare_range_);

			item_id_spare_range_.min = 0;
			item_id_spare_range_.max = 0;
			item_id_spare_range_.usable_item_id_min = 0;
		}
	}

	return (current_id_++);
}

bool item_manager::set_max_item_id(item_id_range_table range)
{
	item_id_range_ = range;

	if ( item_id_range_.min == 0 || item_id_range_.max == 0 || item_id_range_.usable_item_id_min == 0 )
	{
		metin2::log::error("ItemIDRange: FATAL ERROR!!! ITEM ID RANGE is not set.");
		touch(".killscript");
		thecore_shutdown();
		return false;
	}

	current_id_ = range.usable_item_id_min;

	metin2::log::info( "ItemIDRange: {:d} ~ {:d} {:d}", item_id_range_.min, item_id_range_.max, current_id_);

	return true;
}

bool item_manager::set_max_spare_item_id(item_id_range_table range)
{
	if ( range.min == 0 || range.max == 0 || range.usable_item_id_min == 0 )
	{
		metin2::log::error("ItemIDRange: FATAL ERROR!!! Spare ITEM ID RANGE is not set");
		return false;
	}

	item_id_spare_range_ = range;

	metin2::log::info( "ItemIDRange: New Spare ItemID Range Recv {:d} ~ {:d} {:d}",
			item_id_spare_range_.min, item_id_spare_range_.max, item_id_spare_range_.usable_item_id_min);

	return true;
}
}
