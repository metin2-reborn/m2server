#pragma once

#include "event.h"
#include "item.h"
#include "typedef.h"

#include <cstdint>

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

	extern void Initialize();
	extern LPEVENT CreateFishingEvent(CHARACTER * ch);
	extern void Take(fishing_event_info* info, CHARACTER * ch);
	extern void Simulation(int level, int count, int map_grade, CHARACTER * ch);
	extern void UseFish(CHARACTER * ch, CItem * item);
	extern void Grill(CHARACTER * ch, CItem * item);

	extern bool RefinableRod(CItem * rod);
	extern int RealRefineRod(CHARACTER * ch, CItem * rod);
}
