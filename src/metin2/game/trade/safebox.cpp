#include <metin2/game/trade/safebox.h>

#include <metin2/game/characters/character.h>
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

safebox::safebox(character * chr_owner, int size, uint32_t gold) : chr_owner_(chr_owner), size_(size), gold_(gold)
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

void safebox::set_window_mode(uint8_t mode)
{
	window_mode_ = mode;
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

auto safebox::add(uint32_t pos, item * item_value) -> bool
{
	if (!is_valid_position(pos))
	{
		metin2::log::error("Safebox cell {:d} is out of range (grid size {:d})", pos, grid_->size());
		return false;
	}

	item_value->set_window(window_mode_);
	item_value->set_cell(chr_owner_, pos);
	item_value->save();
	item_manager::instance().flush_delayed_save(item_value);

	grid_->put(pos, 1, item_value->size());
	items_[pos] = item_value;

	send_item_set(pos);
	metin2::log::debug("Safebox: {} added {} (count {:d})", chr_owner_->name(), item_value->name(), item_value->count());
	return true;
}

void safebox::send_item_set(uint32_t pos)
{
	item * pkItem = items_[pos];

	if (!pkItem || !chr_owner_->get_desc())
		return;

	packet_gc_item_set pack;

	pack.header	= window_mode_ == SAFEBOX ? HEADER_GC_SAFEBOX_SET : HEADER_GC_MALL_SET;
	pack.cell	= item_pos(window_mode_, pos);
	pack.vnum	= pkItem->vnum();
	pack.count	= pkItem->count();
	pack.flags	= pkItem->flag();
	pack.anti_flags	= pkItem->anti_flag();
	std::memcpy(pack.sockets, pkItem->sockets(), sizeof(pack.sockets));
	std::memcpy(pack.attr, pkItem->attributes(), sizeof(pack.attr));

	chr_owner_->get_desc()->packet(&pack, sizeof(pack));
}

auto safebox::get(uint32_t pos) -> item *
{
	if (!grid_ || pos >= grid_->size())
		return nullptr;

	return items_[pos];
}

auto safebox::remove(uint32_t pos_value) -> item *
{
	item * pkItem = get(pos_value);

	if (!pkItem)
		return nullptr;

	if (!grid_)
		metin2::log::error("Safebox has no grid");
	else
		grid_->get(pos_value, 1, pkItem->size());

	pkItem->remove_from_character();

	items_[pos_value] = nullptr;

	packet_gc_item_del pack;

	pack.header	= window_mode_ == SAFEBOX ? HEADER_GC_SAFEBOX_DEL : HEADER_GC_MALL_DEL;
	pack.pos	= pos_value;

	chr_owner_->get_desc()->packet(&pack, sizeof(pack));
	metin2::log::debug("Safebox: {} removed {} (count {:d})", chr_owner_->name(), pkItem->name(), pkItem->count());
	return pkItem;
}

void safebox::save()
{
	safebox_table t;

	memset(&t, 0, sizeof(safebox_table));

	t.id = chr_owner_->get_desc()->get_account_table().id;
	t.gold = gold_;

	db_clientdesc->db_packet(HEADER_GD_SAFEBOX_SAVE, 0, &t, sizeof(safebox_table));
	metin2::log::debug("Safebox: saving for {}", chr_owner_->name());
}

auto safebox::is_empty(uint32_t pos, uint8_t size) -> bool
{
	if (!grid_)
		return false;

	return grid_->is_empty(pos, 1, size);
}

void safebox::change_size(int size)
{
	if (size_ >= size)
		return;

	size_ = size;

	if (grid_)
		grid_ = std::make_unique<grid>(*grid_, 5, size_);
	else
		grid_ = std::make_unique<grid>(5, size_);
}

auto safebox::get_item(uint8_t cell) -> item *
{
	if (cell >= 5 * size_)
	{
		metin2::log::warn("Safebox cell {:d} is out of range", cell);
		return nullptr;
	}

	return items_[cell];
}

auto safebox::move_item(uint8_t cell, uint8_t dest_cell, uint8_t count) -> bool
{
	item * item;

	int max_position = 5 * size_;

	if (cell >= max_position || dest_cell >= max_position)
		return false;

	if (!(item = get_item(cell)))
		return false;

	if (item->is_exchanging())
		return false;

	if (item->count() < count)
		return false;

	{
		game::items::item * item2;

		if ((item2 = get_item(dest_cell)) && item != item2 && item2->is_stackable() &&
				!is_set(item2->anti_flag(), ITEM_ANTIFLAG_STACK) &&
				item2->vnum() == item->vnum())
		{
			for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
				if (item2->get_socket(i) != item->get_socket(i))
					return false;

			if (count == 0)
				count = item->count();

			count = std::min<int>(ITEM_MAX_COUNT - item2->count(), count);

			if (count == 0)
				return false;

			item2->set_count(item2->count() + count);
			send_item_set(dest_cell);

			if (count == item->count())
				item_manager::instance().remove_item(item, "SAFEBOX_STACK");
			else
			{
				item->set_count(item->count() - count);
				send_item_set(cell);
			}

			metin2::log::debug("Safebox: {} stacked cell {:d} onto {:d}, {} count {:d}", chr_owner_->name(), cell, dest_cell, item2->name(), item2->count());
			return true;
		}

		if (!is_empty(dest_cell, item->size()))
			return false;

		grid_->get(cell, 1, item->size());

		if (!grid_->put(dest_cell, 1, item->size()))
		{
			grid_->put(cell, 1, item->size());
			return false;
		}
		else
		{
			grid_->get(dest_cell, 1, item->size());
			grid_->put(cell, 1, item->size());
		}

		metin2::log::debug("Safebox: {} moved cell {:d} to {:d}, {} count {:d}", chr_owner_->name(), cell, dest_cell, item->name(), item->count());

		remove(cell);
		add(dest_cell, item);
	}

	return true;
}

auto safebox::is_valid_position(uint32_t pos) -> bool
{
	if (!grid_)
		return false;

	if (pos >= grid_->size())
		return false;

	return true;
}
}
