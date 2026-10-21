#pragma once

#include <cstdint>

namespace metin2::game::inline protocol {

    inline auto encode_byte(char ind) -> const char * {
        static char a[8];
        *((char *)a) = ind;
        return (a);
    }

    inline auto encode_4bytes(int ind) -> const char * {
        static char a[8];
        *((int *)a) = ind;
        return (a);
    }

    inline auto decode_byte(const void *a) -> uint8_t {
        return (*(uint8_t *)a);
    }

    inline auto decode_2bytes(const void *a) -> uint16_t {
        return (*((uint16_t *)a));
    }

    inline auto decode_4bytes(const void *a) -> int32_t {
        return (*((int32_t *)a));
    }
}
