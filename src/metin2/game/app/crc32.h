#pragma once

#include <zlib.h>

#include <cstddef>
#include <cstdint>

#include <zconf.h>

namespace metin2::game::inline app {

    using crc_t = uint32_t;

    // Standard CRC-32 (IEEE 802.3), as the client computes it for guild mark blocks.
    inline auto get_cr_c32(const char *buffer, size_t count) -> crc_t {
        return static_cast<crc_t>(crc32(0L, reinterpret_cast<const Bytef *>(buffer), static_cast<uInt>(count)));
    }

    inline auto get_fast_hash(const char *key, size_t len) -> crc_t {
        crc_t h = 0;
        for (const char *end = key + len; key < end; ++key) {
            h *= 16777619;
            h ^= static_cast<crc_t>(*key);
        }
        return h;
    }
}
