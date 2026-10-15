#pragma once

#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>

#include <cstdint>

namespace metin2::game::inline events {

namespace mining
{
	auto create_mining_event(character * ch, character * load, int count) -> event_ptr;
	auto get_raw_ore_from_load(uint32_t load_vnum_value) -> uint32_t;
	auto ore_refine(character * ch, character * npc, item * ore, int cost, int pct, item * metinstone_item) -> bool;
	auto get_fraction_count() -> int;

	auto real_refine_pick(character * ch, item * item) -> int;
	void cheat_max_pick(character * ch, item * item);

	auto is_vein_of_ore (uint32_t vnum) -> bool;
}
}
