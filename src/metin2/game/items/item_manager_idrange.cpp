
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/items/item_manager.h>

#include <metin2/proto/tables.h>
#include <metin2/log/log.h>
#include <metin2/core/main.h>

#include <cassert>
#include <cstdint>
#include <cstdio>

namespace metin2::game::inline items {

auto touch(const char *path) -> int
{
	FILE	*fp;

	if ( !(fp = fopen(path, "a")) )
	{
		metin2::log::error("Cannot open {} for writing", path);
		return (-1);
	}

	fclose(fp);
	return 0;
}

auto item_manager::new_id() -> uint32_t
{
	assert(current_id_ != 0);

	if ( current_id_ >= item_id_range_.max )
	{
		if ( item_id_spare_range_.min == 0 || item_id_spare_range_.max == 0 || item_id_spare_range_.usable_item_id_min == 0 )
		{
			metin2::log::error("Item id range is exhausted and no spare range is set");
			touch(".killscript");
			thecore_shutdown();
			return 0;
		}
		else
		{
			metin2::log::info("Item id range full, switching to spare range {:d}-{:d} (next {:d})",
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

auto item_manager::set_max_item_id(item_id_range_table range) -> bool
{
	item_id_range_ = range;

	if ( item_id_range_.min == 0 || item_id_range_.max == 0 || item_id_range_.usable_item_id_min == 0 )
	{
		metin2::log::error("Item id range is not set");
		touch(".killscript");
		thecore_shutdown();
		return false;
	}

	current_id_ = range.usable_item_id_min;

	metin2::log::info("Item id range {:d}-{:d} (next {:d})", item_id_range_.min, item_id_range_.max, current_id_);

	return true;
}

auto item_manager::set_max_spare_item_id(item_id_range_table range) -> bool
{
	if ( range.min == 0 || range.max == 0 || range.usable_item_id_min == 0 )
	{
		metin2::log::error("Spare item id range is not set");
		return false;
	}

	item_id_spare_range_ = range;

	metin2::log::info("Received spare item id range {:d}-{:d} (next {:d})",
			item_id_spare_range_.min, item_id_spare_range_.max, item_id_spare_range_.usable_item_id_min);

	return true;
}
}
