#pragma once

#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>

#include <cstdint>

namespace metin2::game::inline events {

namespace mining
{
	event_ptr create_mining_event(character * ch, character * load, int count);
	uint32_t get_raw_ore_from_load(uint32_t load_vnum_value);
	bool ore_refine(character * ch, character * npc, item * ore, int cost, int pct, item * metinstone_item);
	int get_fraction_count();

	int real_refine_pick(character * ch, item * item);
	void cheat_max_pick(character * ch, item * item);

	bool is_vein_of_ore (uint32_t vnum);
}
}
