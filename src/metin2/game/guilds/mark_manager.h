#pragma once

#include <metin2/game/guilds/mark_image.h>

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace metin2::game::inline guilds {

    class guild_mark_manager {
      public:
        enum {
            MAX_IMAGE_COUNT = 5,
            INVALID_MARK_ID = 0xffffffff,
        };

        struct guild_symbol {
            uint32_t crc;
            std::vector<uint8_t> raw;
        };

        guild_mark_manager();
        ~guild_mark_manager();

        auto get_guild_symbol(uint32_t guild_id) -> const guild_symbol *;
        auto load_symbol(const char *filename) -> bool;
        void save_symbol(const char *filename);
        void upload_symbol(uint32_t guild_id, int size, const uint8_t *pby_data);

        void set_mark_path_prefix(const char *prefix);

        auto load_mark_index() -> bool;
        auto save_mark_index() -> bool;

        void load_mark_images();
        void save_mark_image(uint32_t img_idx);

        auto mark_image_filename(uint32_t img_idx, std::string &path) const -> bool;
        auto add_mark_id_by_guild_id(uint32_t guild_id, uint32_t mark_id) -> bool;
        auto mark_count() const -> uint32_t;
        auto mark_id(uint32_t guild_id) -> uint32_t;

        void copy_mark_idx(char *pc_buf) const;
        auto save_mark(uint32_t guild_id, uint8_t *mark_image) -> uint32_t;
        void delete_mark(uint32_t guild_id);
        void diff_blocks(uint32_t img_idx, const uint32_t *crc_list, std::map<uint8_t, const guild_mark_block *> &diff_blocks_value);

      private:
        auto alloc_mark_id(uint32_t guild_id) -> uint32_t;

        auto image(uint32_t img_idx) -> guild_mark_image *;

        std::map<uint32_t, std::unique_ptr<guild_mark_image>> idx_image_;
        std::map<uint32_t, uint32_t> gid_mark_id_;

        std::set<uint32_t> free_mark_id_;
        std::string path_prefix_;

      private:
        std::map<uint32_t, guild_symbol> symbol_;
    };

    inline auto get_guild_mark_manager() -> guild_mark_manager & {
        static guild_mark_manager instance;
        return instance;
    }
}
