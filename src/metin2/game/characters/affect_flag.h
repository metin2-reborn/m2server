#pragma once

#include <metin2/game/app/utils.h>
#include <metin2/game/characters/affect.h>

#include <cstdint>

namespace metin2::game::inline characters {

    struct affect_flag {
        uint32_t bits[2];

        inline affect_flag() {
            bits[0] = 0;
            bits[1] = 0;
        }

        inline affect_flag(uint32_t v1, uint32_t v2 = 0) {
            bits[0] = v1;
            bits[1] = v2;
        }

        inline auto is_set(int flag) const -> bool {
            if (AFF_BITS_MAX <= flag || 0 >= flag) {
                return false;
            }

            return game::app::is_set(bits[(flag - 1) >> 5], (((uint32_t)1) << ((flag - 1) & 31)));
        }

        inline void set(int flag) {
            if (AFF_BITS_MAX <= flag || 0 >= flag) {
                return;
            }

            set_bit(bits[(flag - 1) >> 5], (((uint32_t)1) << ((flag - 1) & 31)));
        }

        inline void reset(int flag) {
            if (AFF_BITS_MAX <= flag || 0 >= flag) {
                return;
            }

            remove_bit(bits[(flag - 1) >> 5], (((uint32_t)1) << ((flag - 1) & 31)));
        }
    };

    inline auto operator==(const affect_flag &lhs, const affect_flag &rhs) -> bool {
        return lhs.bits[0] == rhs.bits[0] && lhs.bits[1] == rhs.bits[1];
    }

    inline auto operator!=(const affect_flag &lhs, const affect_flag &rhs) -> bool {
        return !(lhs == rhs);
    }
}
