#pragma once

#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>

#include <cstdint>

namespace metin2::game::inline events {

namespace mining
{
	LPEVENT CreateMiningEvent(CHARACTER * ch, CHARACTER * load, int count);
	uint32_t GetRawOreFromLoad(uint32_t dwLoadVnum);
	bool OreRefine(CHARACTER * ch, CHARACTER * npc, CItem * item, int cost, int pct, CItem * metinstone_item);
	int GetFractionCount();

	// REFINE_PICK
	int RealRefinePick(CHARACTER * ch, CItem * item);
	void CHEAT_MAX_PICK(CHARACTER * ch, CItem * item);
	// END_OF_REFINE_PICK

	bool IsVeinOfOre (uint32_t vnum);
}
}
