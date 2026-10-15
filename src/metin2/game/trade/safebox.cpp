#include <metin2/game/trade/safebox.h>

#include <metin2/game/characters/char.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/items/item.h>
#include <metin2/game/items/item_manager.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>
#include <metin2/common/grid.h>
#include <metin2/log/log.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>

namespace metin2::game::inline trade {

safebox::safebox(character * pkChrOwner, int iSize, uint32_t dwGold) : chr_owner_(pkChrOwner), size_(iSize), gold_(dwGold)
{
	assert(chr_owner_ != nullptr);
	memset(items_, 0, sizeof(items_));

	if (size_)
		grid_ = std::make_unique<grid>(5, size_);

	window_mode_ = SAFEBOX;
}

safebox::~safebox()
{
	destroy();
}

void safebox::set_window_mode(uint8_t bMode)
{
	window_mode_ = bMode;
}

void safebox::destroy()
{
	for (int i = 0; i < SAFEBOX_MAX_NUM; ++i)
	{
		if (items_[i])
		{
			items_[i]->set_skip_save(true);
			item_manager::instance().flush_delayed_save(items_[i]);

			item_manager::instance().destroy_item(items_[i]->remove_from_character());
			items_[i] = nullptr;
		}
	}

	grid_.reset();
}

bool safebox::add(uint32_t dwPos, item * pkItem)
{
	if (!is_valid_position(dwPos))
	{
		metin2::log::error("SAFEBOX: item on wrong position at {:d} (size of grid = {:d})", dwPos, grid_->size());
		return false;
	}

	pkItem->set_window(window_mode_);
	pkItem->set_cell(chr_owner_, dwPos);
	pkItem->save(); // Save must be called forcibly.
	item_manager::instance().flush_delayed_save(pkItem);

	grid_->put(dwPos, 1, pkItem->size());
	items_[dwPos] = pkItem;

	send_item_set(dwPos);
	metin2::log::debug( "SAFEBOX: ADD {} {} count {:d}", chr_owner_->name(), pkItem->name(), pkItem->count());
	return true;
}

void safebox::send_item_set(uint32_t dwPos)
{
	item * pkItem = items_[dwPos];

	if (!pkItem || !chr_owner_->get_desc())
		return;

	packet_gc_item_set pack;

	pack.header	= window_mode_ == SAFEBOX ? HEADER_GC_SAFEBOX_SET : HEADER_GC_MALL_SET;
	pack.cell	= item_pos(window_mode_, dwPos);
	pack.vnum	= pkItem->vnum();
	pack.count	= pkItem->count();
	pack.flags	= pkItem->flag();
	pack.anti_flags	= pkItem->anti_flag();
	std::memcpy(pack.sockets, pkItem->sockets(), sizeof(pack.sockets));
	std::memcpy(pack.attr, pkItem->attributes(), sizeof(pack.attr));

	chr_owner_->get_desc()->packet(&pack, sizeof(pack));
}

item * safebox::get(uint32_t dwPos)
{
	if (!grid_ || dwPos >= grid_->size())
		return nullptr;

	return items_[dwPos];
}

item * safebox::remove(uint32_t dwPos)
{
	item * pkItem = get(dwPos);

	if (!pkItem)
		return nullptr;

	if (!grid_)
		metin2::log::error("Safebox::Remove : nil grid");
	else
		grid_->get(dwPos, 1, pkItem->size());

	pkItem->remove_from_character();

	items_[dwPos] = nullptr;

	packet_gc_item_del pack;

	pack.header	= window_mode_ == SAFEBOX ? HEADER_GC_SAFEBOX_DEL : HEADER_GC_MALL_DEL;
	pack.pos	= dwPos;

	chr_owner_->get_desc()->packet(&pack, sizeof(pack));
	metin2::log::debug( "SAFEBOX: REMOVE {} {} count {:d}", chr_owner_->name(), pkItem->name(), pkItem->count());
	return pkItem;
}

void safebox::save()
{
	safebox_table t;

	memset(&t, 0, sizeof(safebox_table));

	t.id = chr_owner_->get_desc()->get_account_table().id;
	t.gold = gold_;

	db_clientdesc->db_packet(HEADER_GD_SAFEBOX_SAVE, 0, &t, sizeof(safebox_table));
	metin2::log::debug( "SAFEBOX: SAVE {}", chr_owner_->name());
}

bool safebox::is_empty(uint32_t dwPos, uint8_t bSize)
{
	if (!grid_)
		return false;

	return grid_->is_empty(dwPos, 1, bSize);
}

void safebox::change_size(int iSize)
{
	// if the current size is larger than the argument, leave the size as is.
	if (size_ >= iSize)
		return;

	size_ = iSize;

	if (grid_)
		grid_ = std::make_unique<grid>(*grid_, 5, size_);
	else
		grid_ = std::make_unique<grid>(5, size_);
}

item * safebox::get_item(uint8_t bCell)
{
	if (bCell >= 5 * size_)
	{
		metin2::log::error("CHARACTER::GetItem: invalid item cell {:d}", bCell);
		return nullptr;
	}

	return items_[bCell];
}

bool safebox::move_item(uint8_t bCell, uint8_t bDestCell, uint8_t count)
{
	item * item;

	int max_position = 5 * size_;

	if (bCell >= max_position || bDestCell >= max_position)
		return false;

	if (!(item = get_item(bCell)))
		return false;

	if (item->is_exchanging())
		return false;

	if (item->count() < count)
		return false;

	{
		game::items::item * item2;

		if ((item2 = get_item(bDestCell)) && item != item2 && item2->is_stackable() &&
				!is_set(item2->anti_flag(), ITEM_ANTIFLAG_STACK) &&
				item2->vnum() == item->vnum()) // for stackable items
		{
			for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
				if (item2->get_socket(i) != item->get_socket(i))
					return false;

			if (count == 0)
				count = item->count();

			count = std::min<int>(ITEM_MAX_COUNT - item2->count(), count);

			// The destination stack is full: nothing moves.
			if (count == 0)
				return false;

			item2->set_count(item2->count() + count);
			send_item_set(bDestCell);

			// The whole source stack merged: it goes away; otherwise the rest stays where it was.
			if (count == item->count())
				item_manager::instance().remove_item(item, "SAFEBOX_STACK");
			else
			{
				item->set_count(item->count() - count);
				send_item_set(bCell);
			}

			metin2::log::debug( "SAFEBOX: STACK {} {:d} -> {:d} {} count {:d}", chr_owner_->name(), bCell, bDestCell, item2->name(), item2->count());
			return true;
		}

		if (!is_empty(bDestCell, item->size()))
			return false;

		grid_->get(bCell, 1, item->size());

		if (!grid_->put(bDestCell, 1, item->size()))
		{
			grid_->put(bCell, 1, item->size());
			return false;
		}
		else
		{
			grid_->get(bDestCell, 1, item->size());
			grid_->put(bCell, 1, item->size());
		}

		metin2::log::debug( "SAFEBOX: MOVE {} {:d} -> {:d} {} count {:d}", chr_owner_->name(), bCell, bDestCell, item->name(), item->count());

		remove(bCell);
		add(bDestCell, item);
	}

	return true;
}

bool safebox::is_valid_position(uint32_t dwPos)
{
	if (!grid_)
		return false;

	if (dwPos >= grid_->size())
		return false;

	return true;
}
}
