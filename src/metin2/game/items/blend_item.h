
#pragma once

#include <metin2/game/app/typedef.h>

#include <cstdint>

namespace metin2::game::inline items {

inline constexpr int MAX_BLEND_ITEM_VALUE = 5;
bool	blend_item_init();
bool	blend_item_load(char *file);
bool	blend_item_set_value(item * item);
bool	blend_item_find(uint32_t item_vnum);
}
