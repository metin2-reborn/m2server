#pragma once

#include <cstdint>
#include <vector>

// Image files as 32-bit pixels in memory order B, G, R, A (0xAARRGGBB as a uint32_t), top row first.

// Reads a TGA or PNG; returns an empty vector when the file is missing or unreadable.
auto load_image_bgra(const char *filename, int &width, int &height) -> std::vector<uint32_t>;

// Writes an uncompressed 32-bit TGA.
auto save_tga_bgra(const char *filename, const uint32_t *pixels, int width, int height) -> bool;
