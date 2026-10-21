#pragma once

#include <metin2/game/text/locale.hpp>

#include <cstdint>

namespace metin2::game::inline empire {

    extern char nation_name[4][32];
    extern uint32_t start_position[4][2];
    extern int32_t start_map[4];
    extern uint32_t create_position[4][2];
    extern uint32_t arena_return_position[4][2];

    inline auto empire_name(uint8_t e) -> const char * {
        return lc_text(nation_name[e]);
    }

    inline auto empire_start_map(uint8_t e) -> uint32_t {
        return start_map[e];
    }

    inline auto empire_start_x(uint8_t e) -> uint32_t {
        if (1 <= e && e <= 3)
            return start_position[e][0];

        return 0;
    }

    inline auto empire_start_y(uint8_t e) -> uint32_t {
        if (1 <= e && e <= 3)
            return start_position[e][1];

        return 0;
    }

    inline auto arena_return_point_x(uint8_t e) -> uint32_t {
        if (1 <= e && e <= 3)
            return arena_return_position[e][0];

        return 0;
    }

    inline auto arena_return_point_y(uint8_t e) -> uint32_t {
        if (1 <= e && e <= 3)
            return arena_return_position[e][1];

        return 0;
    }

    inline auto create_start_x(uint8_t e) -> uint32_t {
        if (1 <= e && e <= 3) {

            return create_position[e][0];
        }

        return 0;
    }

    inline auto create_start_y(uint8_t e) -> uint32_t {
        if (1 <= e && e <= 3) {

            return create_position[e][1];
        }

        return 0;
    }
}
