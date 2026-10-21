#pragma once

#include <cstdint>

namespace metin2::inline proto {
    namespace item_vnum {
        constexpr auto is_ramadan_moon_ring(uint32_t vnum) -> bool {
            return 71135 == vnum;
        }

        constexpr auto is_halloween_candy(uint32_t vnum) -> bool {
            return 71136 == vnum;
        }

        constexpr auto is_happiness_ring(uint32_t vnum) -> bool {
            return 71143 == vnum;
        }

        constexpr auto is_love_pendant(uint32_t vnum) -> bool {
            return 71145 == vnum;
        }
    }

    namespace mob_vnum {
        constexpr auto is_ramadan_black_horse(uint32_t vnum) -> bool {
            return 20119 == vnum ||
                   20219 == vnum ||
                   22022 == vnum;
        }
    }
}
