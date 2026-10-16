#include <metin2/game/guilds/mark_image.h>

#include <metin2/game/app/crc32.h>
#include <metin2/game/data/image_file.h>
#include <metin2/game/protocol/lzo_manager.h>

#include <metin2/log/log.h>

#include <lzo/lzo1x.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <utility>
#include <vector>

namespace metin2::game::inline guilds {

    guild_mark_image::guild_mark_image() : pixels_(WIDTH * HEIGHT) {
    }

    auto guild_mark_image::save(const char *file_name) -> bool {
        return save_tga_bgra(file_name, pixels_.data(), WIDTH, HEIGHT);
    }

    auto guild_mark_image::build(const char *file_name) -> bool {
        metin2::log::info("Guild marks: creating new mark image {}", file_name);

        std::ranges::fill(pixels_, 0);
        return save(file_name);
    }

    auto guild_mark_image::load(const char *file_name) -> bool {
        int width = 0;
        int height = 0;
        std::vector<pixel> pixels = load_image_bgra(file_name, width, height);

        if (pixels.empty()) {
            metin2::log::error("Cannot open guild mark image {}", file_name);
            return false;
        }

        if (width != WIDTH) {
            metin2::log::error("Guild mark image {} has the wrong width, expected {:d}", file_name, WIDTH);
            return false;
        }

        if (height != HEIGHT) {
            metin2::log::error("Guild mark image {} has the wrong height, expected {:d}", file_name, HEIGHT);
            return false;
        }

        pixels_ = std::move(pixels);
        build_all_blocks();
        return true;
    }

    void guild_mark_image::put_data(uint32_t x, uint32_t y, uint32_t width, uint32_t height, void *data) {
        const auto *src = static_cast<const pixel *>(data);
        for (uint32_t row = 0; row < height && y + row < HEIGHT; ++row)
            std::copy_n(src + row * width, std::min<uint32_t>(width, WIDTH - std::min<uint32_t>(x, WIDTH)), pixels_.begin() + (y + row) * WIDTH + x);
    }

    void guild_mark_image::data(uint32_t x, uint32_t y, uint32_t width, uint32_t height, void *data) {
        auto *dst = static_cast<pixel *>(data);
        for (uint32_t row = 0; row < height && y + row < HEIGHT; ++row)
            std::copy_n(pixels_.begin() + (y + row) * WIDTH + x, std::min<uint32_t>(width, WIDTH - std::min<uint32_t>(x, WIDTH)), dst + row * width);
    }

    auto guild_mark_image::save_mark(uint32_t pos_mark, uint8_t *image) -> bool {
        if (pos_mark >= MARK_TOTAL_COUNT) {
            metin2::log::error("Invalid guild mark position {:d}", pos_mark);
            return false;
        }

        uint32_t colMark = pos_mark % MARK_COL_COUNT;
        uint32_t rowMark = pos_mark / MARK_COL_COUNT;

        printf("PutMark pos %u %ux%u\n", pos_mark, colMark * guild_mark::WIDTH, rowMark * guild_mark::HEIGHT);
        put_data(colMark * guild_mark::WIDTH, rowMark * guild_mark::HEIGHT, guild_mark::WIDTH, guild_mark::HEIGHT, image);

        uint32_t rowBlock = rowMark / guild_mark_block::MARK_PER_BLOCK_HEIGHT;
        uint32_t colBlock = colMark / guild_mark_block::MARK_PER_BLOCK_WIDTH;

        pixel apxBuf[guild_mark_block::SIZE];
        data(colBlock * guild_mark_block::WIDTH, rowBlock * guild_mark_block::HEIGHT, guild_mark_block::WIDTH, guild_mark_block::HEIGHT, apxBuf);
        aak_block_[rowBlock][colBlock].compress(apxBuf);
        return true;
    }

    auto guild_mark_image::delete_mark(uint32_t pos_mark) -> bool {
        pixel image[guild_mark::SIZE];
        memset(&image, 0, sizeof(image));
        return save_mark(pos_mark, (uint8_t *)&image);
    }

    void guild_mark_image::build_all_blocks() {
        pixel apxBuf[guild_mark_block::SIZE];
        metin2::log::debug("Guild marks: building all mark blocks");

        for (uint32_t row = 0; row < BLOCK_ROW_COUNT; ++row)
            for (uint32_t col = 0; col < BLOCK_COL_COUNT; ++col) {
                data(col * guild_mark_block::WIDTH, row * guild_mark_block::HEIGHT, guild_mark_block::WIDTH, guild_mark_block::HEIGHT, apxBuf);
                aak_block_[row][col].compress(apxBuf);
            }
    }

    void guild_mark_image::diff_blocks(const uint32_t *crc_list, std::map<uint8_t, const guild_mark_block *> &diff_blocks) {
        uint8_t posBlock = 0;

        for (uint32_t row = 0; row < BLOCK_ROW_COUNT; ++row)
            for (uint32_t col = 0; col < BLOCK_COL_COUNT; ++col) {
                if (aak_block_[row][col].crc != *crc_list) {
                    diff_blocks.insert(std::map<uint8_t, const guild_mark_block *>::value_type(posBlock, &aak_block_[row][col]));
                }
                ++crc_list;
                ++posBlock;
            }
    }

    void guild_mark_block::compress(const pixel *px_buf) {
        size_comp_buf = MAX_COMP_SIZE;

        if (LZO_E_OK != lzo1x_1_compress((const uint8_t *)px_buf, sizeof(pixel) * guild_mark_block::SIZE, comp_buf, &size_comp_buf, lzo_manager::instance().work_memory())) {
            metin2::log::error("Guild mark block compression failed ({:d} bytes in, {:d} bytes out)", sizeof(pixel) * guild_mark_block::SIZE, static_cast<unsigned long>(size_comp_buf));
            return;
        }

        crc = get_cr_c32((const char *)px_buf, sizeof(pixel) * guild_mark_block::SIZE);
    }
}
