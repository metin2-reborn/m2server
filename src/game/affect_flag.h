#pragma once

#include "affect.h"
#include "utils.h"

#include <cstdint>

struct TAffectFlag {
    uint32_t bits[2];

    inline TAffectFlag() {
        bits[0] = 0;
        bits[1] = 0;
    }

    inline TAffectFlag(uint32_t v1, uint32_t v2 = 0) {
        bits[0] = v1;
        bits[1] = v2;
    }

    inline auto IsSet(int flag) const -> bool {
        if (AFF_BITS_MAX <= flag || 0 >= flag) {
            return false;
        }

        return IS_SET(bits[(flag - 1) >> 5], (((uint32_t)1) << ((flag - 1) & 31)));
    }

    inline void Set(int flag) {
        if (AFF_BITS_MAX <= flag || 0 >= flag) {
            return;
        }

        SET_BIT(bits[(flag - 1) >> 5], (((uint32_t)1) << ((flag - 1) & 31)));
    }

    inline void Reset(int flag) {
        if (AFF_BITS_MAX <= flag || 0 >= flag) {
            return;
        }

        REMOVE_BIT(bits[(flag - 1) >> 5], (((uint32_t)1) << ((flag - 1) & 31)));
    }

    inline auto operator=(const TAffectFlag &rhs) -> TAffectFlag & {
        bits[0] = rhs.bits[0];
        bits[1] = rhs.bits[1];
        return *this;
    }
};

inline auto operator==(const TAffectFlag &lhs, const TAffectFlag &rhs) -> bool {
    return lhs.bits[0] == rhs.bits[0] && lhs.bits[1] == rhs.bits[1];
}

inline auto operator!=(const TAffectFlag &lhs, const TAffectFlag &rhs) -> bool {
    return !(lhs == rhs);
}
