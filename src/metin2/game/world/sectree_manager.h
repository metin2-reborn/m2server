#pragma once

#include <metin2/game/app/typedef.h>
#include <metin2/game/world/sectree.h>

#include <metin2/log/log.h>

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace metin2::game::inline world {

    struct map_region {
        int index;
        int sx, sy, ex, ey;
        pixel_position pos_spawn;

        bool empire_spawn_different;
        pixel_position pos_empire[3];

        std::string map_name;
    };

    struct area_info {
        int sx, sy, ex, ey, dir;
        area_info(int sx, int sy, int ex, int ey, int dir)
            : sx(sx), sy(sy), ex(ex), ey(ey), dir(dir) {}
    };

    struct npc_info {
        uint8_t type;
        const char *name;
        int32_t x, y;
        npc_info(uint8_t type_value, const char *name, int32_t x, int32_t y)
            : type(type_value), name(name), x(x), y(y) {}
    };

    using area_map = std::map<std::string, area_info>;

    struct map_setting {
        int index;
        int cell_scale;
        int base_x;
        int base_y;
        int width;
        int height;

        pixel_position pos_spawn;
    };

    class sectree_map {
      public:
        using map_type = std::map<uint32_t, std::unique_ptr<sectree>>;

        sectree_map();
        sectree_map(sectree_map &r);
        virtual ~sectree_map();

        auto add(uint32_t key, std::unique_ptr<sectree> sectree) -> bool {
            return map_.emplace(key, std::move(sectree)).second;
        }

        auto find(uint32_t package) -> sectree *;
        auto find(uint32_t x, uint32_t y) -> sectree *;
        void build();

        map_setting setting;

        template <typename Func>
        void for_each(Func &&rfunc) {
            std::vector<entity *> collected;
            for (auto &[key, sectree] : map_)
                sectree->for_each_entity([&collected](entity *e) { collected.push_back(e); });
            for (entity *e : collected)
                rfunc(e);
        }

        void dump_all_to_sys_err() {
            sectree_map::map_type::iterator i;
            for (i = map_.begin(); i != map_.end(); ++i) {
                metin2::log::error("Sectree {:x} ({:d}, {:d})", i->first, i->first & 0xffff, i->first >> 16);
            }
        }

      private:
        map_type map_;
    };

    enum attr_region_mode {
        ATTR_REGION_MODE_SET,
        ATTR_REGION_MODE_REMOVE,
        ATTR_REGION_MODE_CHECK,
    };

    class sectree_manager {
      public:
        sectree_manager();
        ~sectree_manager();

        auto map(int32_t map_index) -> sectree_map *;
        auto get(uint32_t index, uint32_t package) -> sectree *;
        auto get(uint32_t index, uint32_t x, uint32_t y) -> sectree *;

        template <typename Func>
        void for_each(int map_index, Func &&rfunc) {
            sectree_map *sec_map = map(map_index);
            if (sec_map) {
                sec_map->for_each(rfunc);
            }
        }

        auto load_setting_file(int32_t map_index, const char *setting_file_name, map_setting &setting) -> int;
        auto load_map_region(const char *file_name, map_setting &setting, const char *map_name_value) -> bool;
        auto build(const char *list_file_name, const char *map_base_path) -> int;
        auto build_sectree_from_setting(map_setting &setting) -> std::unique_ptr<sectree_map>;
        auto load_attribute(sectree_map *map_sectree, const char *file_name, map_setting &setting) -> bool;
        void load_dungeon(int index, const char *file_name);
        auto valid_location(int32_t map_index, int32_t x, int32_t y, int32_t &valid_map_index, pixel_position &pos, uint8_t empire = 0) -> bool;
        auto recall_position_by_empire(int map_index, uint8_t empire, pixel_position &pos) -> bool;

        auto get_map_region(int32_t map_index) -> const map_region *;
        auto map_index(int32_t x, int32_t y) -> int;
        auto find_region_by_partial_name(const char *map_name_value) -> const map_region *;

        auto map_base_position(int32_t x, int32_t y, pixel_position &pos) -> bool;
        auto map_base_position_by_map_index(int32_t map_index, pixel_position &pos) -> bool;
        auto movable_position(int32_t map_index, int32_t x, int32_t y, pixel_position &pos) -> bool;
        auto is_movable_position(int32_t map_index, int32_t x, int32_t y) -> bool;
        // Whether the straight line between the two points crosses no cell with one of the flags;
        // the start cell itself is not tested, so a character already inside one can still leave.
        auto is_path_clear(int32_t map_index, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t flags) -> bool;
        auto center_position_of_map(int32_t map_index, pixel_position &pos) -> bool;
        auto random_location(int32_t map_index, pixel_position &pos, uint32_t current_x = 0, uint32_t current_y = 0, int max_distance = 0) -> bool;

        auto create_private_map(int32_t map_index) -> int32_t;
        void destroy_private_map(int32_t map_index);

        auto dungeon_area(int32_t map_index) -> area_map &;
        void send_npc_position(character *ch);
        void insert_npc_position(int32_t map_index, uint8_t type, const char *name, int32_t x, int32_t y);

        auto empire_from_map_index(int32_t map_index) -> uint8_t;

        void purge_monsters_in_map(int32_t map_index);
        void purge_stones_in_map(int32_t map_index);
        auto monster_count_in_map(int32_t map_index) -> size_t;
        auto monster_count_in_map(int32_t map_index, uint32_t vnum) -> size_t;

        auto for_attr_region(int32_t map_index, int32_t start_x, int32_t start_y, int32_t end_x, int32_t end_y, int32_t rotate, uint32_t attr, attr_region_mode mode) -> bool;

        auto save_attribute_to_image(int map_index, const char *file_name, sectree_map *map_src = nullptr) -> bool;

      private:
        auto for_attr_region_right_angle(int32_t map_index, int32_t cx, int32_t cy, int32_t cw, int32_t ch, int32_t rotate, uint32_t attr, attr_region_mode mode) -> bool;

        auto for_attr_region_free_angle(int32_t map_index, int32_t cx, int32_t cy, int32_t cw, int32_t ch, int32_t rotate, uint32_t attr, attr_region_mode mode) -> bool;

        auto for_attr_region_cell(int32_t map_index, int32_t cx, int32_t cy, uint32_t attr, attr_region_mode mode) -> bool;

        static uint16_t current_sectree_version_;
        std::map<uint32_t, std::unique_ptr<sectree_map>> sectrees_;
        std::map<int, area_map> area_by_map_;
        std::vector<map_region> map_regions_;
        std::map<uint32_t, std::vector<npc_info>> npc_position_;

        using private_index_map_type = std::unordered_map<int32_t, int>;
        private_index_map_type next_private_index_map_;
    };

    inline auto get_sectree_manager() -> sectree_manager & {
        static sectree_manager instance;
        return instance;
    }
}
