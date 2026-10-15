#include <metin2/game/characters/char.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/items/item.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/app/typedef.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>

#include <cstdint>
#include <cstring>

namespace metin2::game::inline characters {

/////////////////////////////////////////////////////////////////////////////
// QUICKSLOT HANDLING
/////////////////////////////////////////////////////////////////////////////
void character::sync_quickslot(uint8_t bType, uint8_t bOldPos, uint8_t bNewPos) // bNewPos == 255 means DELETE
{
	if (bOldPos == bNewPos)
		return;

	for (int i = 0; i < QUICKSLOT_MAX_NUM; ++i)
	{
		if (quickslot_[i].type == bType && quickslot_[i].pos == bOldPos)
		{
			if (bNewPos == 255)
				del_quickslot(i);
			else
			{
				quickslot slot;

				slot.type = bType;
				slot.pos = bNewPos;

				set_quickslot(i, slot);
			}
		}
	}
}

bool character::get_quickslot(uint8_t pos, quickslot ** ppSlot)
{
	if (pos >= QUICKSLOT_MAX_NUM)
		return false;

	*ppSlot = &quickslot_[pos];
	return true;
}

bool character::set_quickslot(uint8_t pos, quickslot & rSlot)
{
	struct packet_quickslot_add pack_quickslot_add;

	if (pos >= QUICKSLOT_MAX_NUM)
		return false;

	if (rSlot.type >= QUICKSLOT_TYPE_MAX_NUM)
		return false;

	for (int i = 0; i < QUICKSLOT_MAX_NUM; ++i)
	{
		if (rSlot.type == 0)
			continue;
		else if (quickslot_[i].type == rSlot.type && quickslot_[i].pos == rSlot.pos)
			del_quickslot(i);
	}

	item_pos srcCell(INVENTORY, rSlot.pos);

	switch (rSlot.type)
	{
		case QUICKSLOT_TYPE_ITEM:
			if (false == srcCell.is_default_inventory_position() && false == srcCell.is_belt_inventory_position())
				return false;

			break;

		case QUICKSLOT_TYPE_SKILL:
			if ((int) rSlot.pos >= SKILL_MAX_NUM)
				return false;

			break;

		case QUICKSLOT_TYPE_COMMAND:
			break;

		default:
			return false;
	}

	quickslot_[pos] = rSlot;

	if (get_desc())
	{
		pack_quickslot_add.header	= HEADER_GC_QUICKSLOT_ADD;
		pack_quickslot_add.pos		= pos;
		pack_quickslot_add.slot		= quickslot_[pos];

		get_desc()->packet(&pack_quickslot_add, sizeof(pack_quickslot_add));
	}

	return true;
}

bool character::del_quickslot(uint8_t pos)
{
	struct packet_quickslot_del pack_quickslot_del;

	if (pos >= QUICKSLOT_MAX_NUM)
		return false;

	memset(&quickslot_[pos], 0, sizeof(quickslot));

	pack_quickslot_del.header	= HEADER_GC_QUICKSLOT_DEL;
	pack_quickslot_del.pos	= pos;

	get_desc()->packet(&pack_quickslot_del, sizeof(pack_quickslot_del));
	return true;
}

bool character::swap_quickslot(uint8_t a, uint8_t b)
{
	struct packet_quickslot_swap pack_quickslot_swap;
	quickslot quickslot;

	if (a >= QUICKSLOT_MAX_NUM || b >= QUICKSLOT_MAX_NUM)
		return false;

	// Swap the quick slot positions.
	quickslot = quickslot_[a];

	quickslot_[a] = quickslot_[b];
	quickslot_[b] = quickslot;

	pack_quickslot_swap.header	= HEADER_GC_QUICKSLOT_SWAP;
	pack_quickslot_swap.pos	= a;
	pack_quickslot_swap.pos_to	= b;

	get_desc()->packet(&pack_quickslot_swap, sizeof(pack_quickslot_swap));
	return true;
}

void character::chain_quickslot_item(item * pItem, uint8_t bType, uint8_t bOldPos)
{
	if (pItem->is_dragon_soul())
		return;
	for ( int i=0; i < QUICKSLOT_MAX_NUM; ++i )
	{
		if ( quickslot_[i].type == bType && quickslot_[i].pos == bOldPos )
		{
			quickslot slot;
			slot.type = bType;
			slot.pos = pItem->cell();

			set_quickslot(i, slot);

			break;
		}
	}
}
}
