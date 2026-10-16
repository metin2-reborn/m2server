#include <metin2/game/guilds/mark_manager.h>

#include <metin2/game/app/crc32.h>
#include <metin2/game/guilds/mark_image.h>

#include <metin2/log/log.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <format>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

namespace metin2::game::inline guilds {

    guild_mark_manager::guild_mark_manager() {
        for (uint32_t i = 0; i < MAX_IMAGE_COUNT * static_cast<uint32_t>(guild_mark_image::MARK_TOTAL_COUNT); ++i)
            free_mark_id_.insert(i);
    }

    guild_mark_manager::~guild_mark_manager() = default;

    auto guild_mark_manager::mark_image_filename(uint32_t img_idx, std::string &path) const -> bool {
        if (img_idx >= MAX_IMAGE_COUNT)
            return false;

        path = std::format("mark/{}_{}.tga", path_prefix_, img_idx);
        return true;
    }

    void guild_mark_manager::set_mark_path_prefix(const char *prefix) {
        path_prefix_ = prefix;
    }

    auto guild_mark_manager::load_mark_index() -> bool {
        const std::string filename = std::format("mark/{}_index", path_prefix_);
        FILE *fp = fopen(filename.c_str(), "r");

        if (!fp)
            return false;

        uint32_t guild_id;
        uint32_t mark_id;

        char line[256];

        while (fgets(line, sizeof(line) - 1, fp)) {
            sscanf(line, "%u %u", &guild_id, &mark_id);
            line[0] = '\0';
            add_mark_id_by_guild_id(guild_id, mark_id);
        }

        load_mark_images();

        fclose(fp);
        return true;
    }

    auto guild_mark_manager::save_mark_index() -> bool {
        const std::string filename = std::format("mark/{}_index", path_prefix_);
        FILE *fp = fopen(filename.c_str(), "w");

        if (!fp) {
            metin2::log::error("Cannot open guild mark index file for writing");
            return false;
        }

        for (auto it = gid_mark_id_.begin(); it != gid_mark_id_.end(); ++it)
            fprintf(fp, "%u %u\n", it->first, it->second);

        fclose(fp);
        metin2::log::info("Guild marks: saved mark index with {:d} entries", gid_mark_id_.size());
        return true;
    }

    void guild_mark_manager::load_mark_images() {
        bool is_mark_exists[MAX_IMAGE_COUNT];
        memset(is_mark_exists, 0, sizeof(is_mark_exists));

        for (auto it = gid_mark_id_.begin(); it != gid_mark_id_.end(); ++it) {
            uint32_t mark_id = it->second;

            if (mark_id < MAX_IMAGE_COUNT * static_cast<uint32_t>(guild_mark_image::MARK_TOTAL_COUNT))
                is_mark_exists[mark_id / guild_mark_image::MARK_TOTAL_COUNT] = true;
        }

        for (uint32_t i = 0; i < MAX_IMAGE_COUNT; ++i)
            if (is_mark_exists[i])
                image(i);
    }

    void guild_mark_manager::save_mark_image(uint32_t img_idx) {
        std::string path;

        if (mark_image_filename(img_idx, path))
            if (!image(img_idx)->save(path.c_str()))
                metin2::log::error("Failed to save guild mark image {}", path);
    }

    auto guild_mark_manager::image(uint32_t img_idx) -> guild_mark_image * {
        auto it = idx_image_.find(img_idx);

        if (it == idx_image_.end()) {
            std::string image_path;

            if (mark_image_filename(img_idx, image_path)) {
                guild_mark_image *image = idx_image_.emplace(img_idx, std::make_unique<guild_mark_image>()).first->second.get();

                if (!image->load(image_path.c_str())) {
                    image->build(image_path.c_str());
                    image->load(image_path.c_str());
                }

                return image;
            } else
                return nullptr;
        } else
            return it->second.get();
    }

    auto guild_mark_manager::add_mark_id_by_guild_id(uint32_t guild_id, uint32_t mark_id) -> bool {
        if (mark_id >= MAX_IMAGE_COUNT * static_cast<uint32_t>(guild_mark_image::MARK_TOTAL_COUNT))
            return false;

        gid_mark_id_.insert(std::map<uint32_t, uint32_t>::value_type(guild_id, mark_id));
        free_mark_id_.erase(mark_id);
        return true;
    }

    auto guild_mark_manager::mark_id(uint32_t guild_id) -> uint32_t {
        auto it = gid_mark_id_.find(guild_id);

        if (it == gid_mark_id_.end())
            return INVALID_MARK_ID;

        return it->second;
    }

    auto guild_mark_manager::alloc_mark_id(uint32_t guild_id) -> uint32_t {
        auto it = free_mark_id_.lower_bound(0);

        if (it == free_mark_id_.end())
            return INVALID_MARK_ID;

        uint32_t mark_id = *it;

        uint32_t img_idx = mark_id / guild_mark_image::MARK_TOTAL_COUNT;
        guild_mark_image *image_value = image(img_idx);

        if (image_value && add_mark_id_by_guild_id(guild_id, mark_id))
            return mark_id;

        return INVALID_MARK_ID;
    }

    auto guild_mark_manager::mark_count() const -> uint32_t {
        return gid_mark_id_.size();
    }

    void guild_mark_manager::copy_mark_idx(char *pc_buf) const {
        auto *buf = (uint16_t *)pc_buf;

        for (auto it = gid_mark_id_.begin(); it != gid_mark_id_.end(); ++it) {
            *(buf++) = it->first;
            *(buf++) = it->second;
        }
    }

    auto guild_mark_manager::save_mark(uint32_t guild_id, uint8_t *mark_image) -> uint32_t {
        uint32_t id_mark;

        if ((id_mark = mark_id(guild_id)) == INVALID_MARK_ID) {
            if ((id_mark = alloc_mark_id(guild_id)) == INVALID_MARK_ID) {
                metin2::log::error("Cannot allocate a mark id for guild {:d}, no free slots", guild_id);
                return false;
            } else
                metin2::log::info("Guild marks: allocated mark id {:d}", id_mark);
        } else
            metin2::log::debug("Guild marks: reusing mark id {:d}", id_mark);

        uint32_t img_idx = (id_mark / guild_mark_image::MARK_TOTAL_COUNT);
        guild_mark_image *image_value = image(img_idx);

        if (image_value) {
            image_value->save_mark(id_mark % guild_mark_image::MARK_TOTAL_COUNT, mark_image);

            save_mark_image(img_idx);
            save_mark_index();
        }

        return id_mark;
    }

    void guild_mark_manager::delete_mark(uint32_t guild_id) {
        auto it = gid_mark_id_.find(guild_id);

        if (it == gid_mark_id_.end())
            return;

        guild_mark_image *image_value;

        if ((image_value = image(it->second / guild_mark_image::MARK_TOTAL_COUNT)) != nullptr)
            image_value->delete_mark(it->second % guild_mark_image::MARK_TOTAL_COUNT);

        free_mark_id_.insert(it->second);
        gid_mark_id_.erase(it);

        save_mark_index();
    }

    void guild_mark_manager::diff_blocks(uint32_t img_idx, const uint32_t *crc_list, std::map<uint8_t, const guild_mark_block *> &diff_blocks_value) {
        diff_blocks_value.clear();

        if (idx_image_.end() == idx_image_.find(img_idx)) {
            metin2::log::error("Invalid guild mark image index {:d}", img_idx);
            return;
        }

        guild_mark_image *p = image(img_idx);

        if (p)
            p->diff_blocks(crc_list, diff_blocks_value);
    }

    auto guild_mark_manager::get_guild_symbol(uint32_t guild_id) -> const guild_mark_manager::guild_symbol * {
        auto it = symbol_.find(guild_id);

        if (it == symbol_.end())
            return nullptr;

        return &it->second;
    }

    auto guild_mark_manager::load_symbol(const char *filename) -> bool {
        FILE *fp = fopen(filename, "rb");

        if (!fp)
            return true;
        else {
            uint32_t symbol_count;
            fread(&symbol_count, 4, 1, fp);

            for (uint32_t i = 0; i < symbol_count; i++) {
                uint32_t guild_id;
                uint32_t size;
                fread(&guild_id, 4, 1, fp);
                fread(&size, 4, 1, fp);

                guild_symbol gs;
                gs.raw.resize(size);
                fread(&gs.raw[0], 1, size, fp);
                gs.crc = get_cr_c32(reinterpret_cast<const char *>(&gs.raw[0]), size);
                symbol_.insert(std::make_pair(guild_id, gs));
            }
        }

        fclose(fp);
        return true;
    }

    void guild_mark_manager::save_symbol(const char *filename) {
        FILE *fp = fopen(filename, "wb");
        if (!fp) {
            metin2::log::error("Cannot open guild symbol file {}", filename);
            return;
        }

        uint32_t symbol_count = symbol_.size();
        fwrite(&symbol_count, 4, 1, fp);

        for (auto it = symbol_.begin(); it != symbol_.end(); ++it) {
            uint32_t guild_id = it->first;
            uint32_t size = it->second.raw.size();
            fwrite(&guild_id, 4, 1, fp);
            fwrite(&size, 4, 1, fp);
            fwrite(&it->second.raw[0], 1, size, fp);
        }

        fclose(fp);
    }

    void guild_mark_manager::upload_symbol(uint32_t guild_id, int size, const uint8_t *pby_data) {
        metin2::log::info("Guild marks: guild {:d} uploaded a symbol ({:d} bytes)", guild_id, size);

        if (symbol_.find(guild_id) == symbol_.end())
            symbol_.insert(std::make_pair(guild_id, guild_symbol()));

        guild_symbol &symbol = symbol_[guild_id];
        symbol.raw.clear();

        if (size > 0) {
            symbol.raw.reserve(size);
            std::copy(pby_data, (pby_data + size), std::back_inserter(symbol.raw));
            symbol.crc = get_cr_c32(reinterpret_cast<const char *>(pby_data), size);
        }
    }
}
