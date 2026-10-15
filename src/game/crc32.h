#pragma once

#include <zlib.h>

#include <cstddef>
#include <cstdint>

#include <zconf.h>

using crc_t = uint32_t;

// The standard CRC-32 (IEEE 802.3); the client computes the same for guild mark blocks.
inline auto GetCRC32(const char *buffer, size_t count) -> crc_t {
    return static_cast<crc_t>(crc32(0L, reinterpret_cast<const Bytef *>(buffer), static_cast<uInt>(count)));
}

// FNV-style hash (h = (h * 16777619) ^ byte, from 0), used to key PvP pairs.
inline auto GetFastHash(const char *key, size_t len) -> crc_t {
    crc_t h = 0;
    for (const char *end = key + len; key < end; ++key) {
        h *= 16777619;
        h ^= static_cast<crc_t>(*key);
    }
    return h;
}
