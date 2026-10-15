#pragma once

#include <metin2/game/app/event.h>
#include <metin2/game/items/item.h>
#include <metin2/game/app/typedef.h>

#include <cstdint>

namespace metin2::game::inline events {

namespace fishing
{
	enum
	{
		CAMPFIRE_MOB = 12000,
		FISHER_MOB = 9009,
		FISH_MIND_PILL_VNUM = 27610,
	};

	EVENTINFO(fishing_event_info)
	{
		uint32_t	pid;
		int		step;
		uint32_t	hang_time;
		int		fish_id;

		fishing_event_info()
		: pid(0)
		, step(0)
		, hang_time(0)
		, fish_id(0)
		{
		}
	};

	extern void initialize();
	extern event_ptr create_fishing_event(character * ch);
	extern void take(fishing_event_info* info, character * ch);
	extern void simulation(int level, int count, int prob_idx, character * ch);
	extern void use_fish(character * ch, item * item);
	extern void grill(character * ch, item * item);

	extern bool refinable_rod(item * rod);
	extern int real_refine_rod(character * ch, item * item);
}
}
