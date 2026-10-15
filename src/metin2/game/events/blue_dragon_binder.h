#pragma once

#include <cstddef>

namespace metin2::game::inline events {

enum bluedragon_stone_effect
{
	DEF_BONUS	=	1,
	ATK_BONUS	=	2,
	REGEN_TIME_BONUS	=	3,
	REGEN_PECT_BONUS	=	4,
};

extern unsigned int blue_dragon_get_range_factor (const char* key, const int val);
extern unsigned int blue_dragon_get_skill_factor (const size_t cnt, ...);
extern unsigned int blue_dragon_get_index_factor (const char* container, const size_t idx, const char* key);
}
