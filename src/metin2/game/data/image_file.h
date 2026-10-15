#pragma once

#include <cstdint>
#include <vector>

namespace metin2::game::inline data {

// Image files as 32-bit pixels in memory order B, G, R, A (0xAARRGGBB as a uint32_t), top row first.

auto load_image_bgra(const char *filename, int &width, int &height) -> std::vector<uint32_t>;

auto save_tga_bgra(const char *filename, const uint32_t *pixels, int width, int height) -> bool;
}
