
#pragma once

#include <metin2/game/app/typedef.h>

#include <cstdint>

namespace metin2::game::inline items {

    inline constexpr int MAX_BLEND_ITEM_VALUE = 5;
    auto blend_item_init() -> bool;
    auto blend_item_load(const char *file) -> bool;
    auto blend_item_set_value(item *item) -> bool;
    auto blend_item_find(uint32_t item_vnum) -> bool;
}
