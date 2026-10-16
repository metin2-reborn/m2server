#include <metin2/game/guilds/mark_manager.h>

#include <metin2/game/app/crc32.h>
#include <metin2/game/guilds/mark_image.h>

#include <metin2/log/log.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
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

    guild_mark_manager::~guild_mark_manager() {
    }

    auto guild_mark_manager::mark_image_filename(uint32_t img_idx, std::string &path) const -> bool {
        if (img_idx >= MAX_IMAGE_COUNT)
            return false;

        char buf[64];
        snprintf(buf, sizeof(buf), "mark/%s_%u.tga", path_prefix_.c_str(), img_idx);
        path = buf;
        return true;
    }

    void guild_mark_manager::set_mark_path_prefix(const char *prefix) {
        path_prefix_ = prefix;
    }

    auto guild_mark_manager::load_mark_index() -> bool {
        char buf[64];
        snprintf(buf, sizeof(buf), "mark/%s_index", path_prefix_.c_str());
        FILE *fp = fopen(buf, "r");

        if (!fp)
            return false;

        uint32_t guildID;
        uint32_t markID;

        char line[256];

        while (fgets(line, sizeof(line) - 1, fp)) {
            sscanf(line, "%u %u", &guildID, &markID);
            line[0] = '\0';
            add_mark_id_by_guild_id(guildID, markID);
        }

        load_mark_images();

        fclose(fp);
        return true;
    }

    auto guild_mark_manager::save_mark_index() -> bool {
        char buf[64];
        snprintf(buf, sizeof(buf), "mark/%s_index", path_prefix_.c_str());
        FILE *fp = fopen(buf, "w");

        if (!fp) {
            metin2::log::error("Cannot open guild mark index file for writing");
            return false;
        }

        for (std::map<uint32_t, uint32_t>::iterator it = gid_mark_id_.begin(); it != gid_mark_id_.end(); ++it)
            fprintf(fp, "%u %u\n", it->first, it->second);

        fclose(fp);
        metin2::log::info("Guild marks: saved mark index with {:d} entries", gid_mark_id_.size());
        return true;
    }

    void guild_mark_manager::load_mark_images() {
        bool isMarkExists[MAX_IMAGE_COUNT];
        memset(isMarkExists, 0, sizeof(isMarkExists));

        for (std::map<uint32_t, uint32_t>::iterator it = gid_mark_id_.begin(); it != gid_mark_id_.end(); ++it) {
            uint32_t markID = it->second;

            if (markID < MAX_IMAGE_COUNT * static_cast<uint32_t>(guild_mark_image::MARK_TOTAL_COUNT))
                isMarkExists[markID / guild_mark_image::MARK_TOTAL_COUNT] = true;
        }

        for (uint32_t i = 0; i < MAX_IMAGE_COUNT; ++i)
            if (isMarkExists[i])
                image(i);
    }

    void guild_mark_manager::save_mark_image(uint32_t img_idx) {
        std::string path;

        if (mark_image_filename(img_idx, path))
            if (!image(img_idx)->save(path.c_str()))
                metin2::log::error("Failed to save guild mark image {}", path.c_str());
    }

    auto guild_mark_manager::image(uint32_t img_idx) -> guild_mark_image * {
        auto it = idx_image_.find(img_idx);

        if (it == idx_image_.end()) {
            std::string imagePath;

            if (mark_image_filename(img_idx, imagePath)) {
                guild_mark_image *pkImage = idx_image_.emplace(img_idx, std::make_unique<guild_mark_image>()).first->second.get();

                if (!pkImage->load(imagePath.c_str())) {
                    pkImage->build(imagePath.c_str());
                    pkImage->load(imagePath.c_str());
                }

                return pkImage;
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
        std::map<uint32_t, uint32_t>::iterator it = gid_mark_id_.find(guild_id);

        if (it == gid_mark_id_.end())
            return INVALID_MARK_ID;

        return it->second;
    }

    auto guild_mark_manager::alloc_mark_id(uint32_t guild_id) -> uint32_t {
        std::set<uint32_t>::iterator it = free_mark_id_.lower_bound(0);

        if (it == free_mark_id_.end())
            return INVALID_MARK_ID;

        uint32_t markID = *it;

        uint32_t imgIdx = markID / guild_mark_image::MARK_TOTAL_COUNT;
        guild_mark_image *pkImage = image(imgIdx);

        if (pkImage && add_mark_id_by_guild_id(guild_id, markID))
            return markID;

        return INVALID_MARK_ID;
    }

    auto guild_mark_manager::mark_count() const -> uint32_t {
        return gid_mark_id_.size();
    }

    void guild_mark_manager::copy_mark_idx(char *pc_buf) const {
        uint16_t *pwBuf = (uint16_t *)pc_buf;

        for (std::map<uint32_t, uint32_t>::const_iterator it = gid_mark_id_.begin(); it != gid_mark_id_.end(); ++it) {
            *(pwBuf++) = it->first;
            *(pwBuf++) = it->second;
        }
    }

    auto guild_mark_manager::save_mark(uint32_t guild_id, uint8_t *mark_image) -> uint32_t {
        uint32_t idMark;

        if ((idMark = mark_id(guild_id)) == INVALID_MARK_ID) {
            if ((idMark = alloc_mark_id(guild_id)) == INVALID_MARK_ID) {
                metin2::log::error("Cannot allocate a mark id for guild {:d}, no free slots", guild_id);
                return false;
            } else
                metin2::log::info("Guild marks: allocated mark id {:d}", idMark);
        } else
            metin2::log::debug("Guild marks: reusing mark id {:d}", idMark);

        uint32_t imgIdx = (idMark / guild_mark_image::MARK_TOTAL_COUNT);
        guild_mark_image *pkImage = image(imgIdx);

        if (pkImage) {
            pkImage->save_mark(idMark % guild_mark_image::MARK_TOTAL_COUNT, mark_image);

            save_mark_image(imgIdx);
            save_mark_index();
        }

        return idMark;
    }

    void guild_mark_manager::delete_mark(uint32_t guild_id) {
        std::map<uint32_t, uint32_t>::iterator it = gid_mark_id_.find(guild_id);

        if (it == gid_mark_id_.end())
            return;

        guild_mark_image *pkImage;

        if ((pkImage = image(it->second / guild_mark_image::MARK_TOTAL_COUNT)) != nullptr)
            pkImage->delete_mark(it->second % guild_mark_image::MARK_TOTAL_COUNT);

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
        std::map<uint32_t, guild_symbol>::iterator it = symbol_.find(guild_id);

        if (it == symbol_.end())
            return nullptr;

        return &it->second;
    }

    auto guild_mark_manager::load_symbol(const char *filename) -> bool {
        FILE *fp = fopen(filename, "rb");

        if (!fp)
            return true;
        else {
            uint32_t symbolCount;
            fread(&symbolCount, 4, 1, fp);

            for (uint32_t i = 0; i < symbolCount; i++) {
                uint32_t guildID;
                uint32_t dwSize;
                fread(&guildID, 4, 1, fp);
                fread(&dwSize, 4, 1, fp);

                guild_symbol gs;
                gs.raw.resize(dwSize);
                fread(&gs.raw[0], 1, dwSize, fp);
                gs.crc = get_cr_c32(reinterpret_cast<const char *>(&gs.raw[0]), dwSize);
                symbol_.insert(std::make_pair(guildID, gs));
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

        uint32_t symbolCount = symbol_.size();
        fwrite(&symbolCount, 4, 1, fp);

        for (std::map<uint32_t, guild_symbol>::iterator it = symbol_.begin(); it != symbol_.end(); ++it) {
            uint32_t guildID = it->first;
            uint32_t dwSize = it->second.raw.size();
            fwrite(&guildID, 4, 1, fp);
            fwrite(&dwSize, 4, 1, fp);
            fwrite(&it->second.raw[0], 1, dwSize, fp);
        }

        fclose(fp);
    }

    void guild_mark_manager::upload_symbol(uint32_t guild_id, int size, const uint8_t *pby_data) {
        metin2::log::info("Guild marks: guild {:d} uploaded a symbol ({:d} bytes)", guild_id, size);

        if (symbol_.find(guild_id) == symbol_.end())
            symbol_.insert(std::make_pair(guild_id, guild_symbol()));

        guild_symbol &rSymbol = symbol_[guild_id];
        rSymbol.raw.clear();

        if (size > 0) {
            rSymbol.raw.reserve(size);
            std::copy(pby_data, (pby_data + size), std::back_inserter(rSymbol.raw));
            rSymbol.crc = get_cr_c32(reinterpret_cast<const char *>(pby_data), size);
        }
    }
}
