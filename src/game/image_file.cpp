#include "image_file.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#define STBI_ONLY_TGA
#define STBI_ONLY_PNG
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#include <stb_image_write.h>

namespace {
// Exchanges the red and blue channels: RGBA bytes <-> BGRA bytes.
auto swap_red_blue(uint32_t pixel) -> uint32_t {
    return (pixel & 0xff00ff00) | ((pixel & 0x00ff0000) >> 16) | ((pixel & 0x000000ff) << 16);
}
} // namespace

auto load_image_bgra(const char *filename, int &width, int &height) -> std::vector<uint32_t> {
    int channels = 0;
    const std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> rgba(stbi_load(filename, &width, &height, &channels, 4), &stbi_image_free);
    if (!rgba) {
        width = height = 0;
        return {};
    }

    std::vector<uint32_t> pixels(static_cast<std::size_t>(width) * height);
    const auto *src = rgba.get();
    for (uint32_t &pixel : pixels) {
        pixel = swap_red_blue(static_cast<uint32_t>(src[0]) | static_cast<uint32_t>(src[1]) << 8 | static_cast<uint32_t>(src[2]) << 16 |
                              static_cast<uint32_t>(src[3]) << 24);
        src += 4;
    }
    return pixels;
}

auto save_tga_bgra(const char *filename, const uint32_t *pixels, int width, int height) -> bool {
    std::vector<stbi_uc> rgba(static_cast<std::size_t>(width) * height * 4);
    for (std::size_t i = 0; i < static_cast<std::size_t>(width) * height; ++i) {
        const uint32_t pixel = swap_red_blue(pixels[i]);
        rgba[i * 4 + 0] = static_cast<stbi_uc>(pixel);
        rgba[i * 4 + 1] = static_cast<stbi_uc>(pixel >> 8);
        rgba[i * 4 + 2] = static_cast<stbi_uc>(pixel >> 16);
        rgba[i * 4 + 3] = static_cast<stbi_uc>(pixel >> 24);
    }

    stbi_write_tga_with_rle = 0;
    return stbi_write_tga(filename, width, height, 4, rgba.data()) != 0;
}
