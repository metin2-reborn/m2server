#include <metin2/config/config.h>
#include <metin2/game/world/sectree_manager.h>

#include <metin2/game/app/config.h>
#include <metin2/game/app/constants.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/data/image_file.h>
#include <metin2/game/items/item.h>
#include <metin2/game/items/item_manager.h>
#include <metin2/game/protocol/buffer_manager.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/desc_manager.h>
#include <metin2/game/protocol/lzo_manager.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/world/entity.h>
#include <metin2/game/world/regen.h>
#include <metin2/game/world/sectree.h>
#include <metin2/game/world/vid.h>

#include <metin2/common/attribute.h>
#include <metin2/core/utils.h>
#include <metin2/log/log.h>

#include <lzo/lzo1x.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <format>
#include <ios>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <strings.h>

namespace metin2::game::inline world {

    uint16_t sectree_manager::current_sectree_version_ = 0x0300;

    sectree_map::sectree_map() {
        memset(&setting, 0, sizeof(setting));
    }

    sectree_map::~sectree_map() = default;

    sectree_map::sectree_map(sectree_map &r) {
        setting = r.setting;

        auto it = r.map_.begin();

        while (it != r.map_.end()) {
            auto tree = std::make_unique<sectree>();

            tree->id_.coord = it->second->id_.coord;
            tree->clone_attribute(it->second.get());

            map_.emplace(it->first, std::move(tree));
            ++it;
        }

        build();
    }

    auto sectree_map::find(uint32_t package) -> sectree * {
        auto it = map_.find(package);

        if (it == map_.end())
            return nullptr;

        return it->second.get();
    }

    auto sectree_map::find(uint32_t x, uint32_t y) -> sectree * {
        sectreeid id;
        id.coord.x = x / SECTREE_SIZE;
        id.coord.y = y / SECTREE_SIZE;
        return find(id.package);
    }

    void sectree_map::build() {
        struct neighbor_coord_s {
            int x;
            int y;
        } neighbor_coord[8] = {
            {-SECTREE_SIZE, 0            },
            {SECTREE_SIZE,  0            },
            {0,             -SECTREE_SIZE},
            {0,             SECTREE_SIZE },
            {-SECTREE_SIZE, SECTREE_SIZE },
            {SECTREE_SIZE,  -SECTREE_SIZE},
            {-SECTREE_SIZE, -SECTREE_SIZE},
            {SECTREE_SIZE,  SECTREE_SIZE },
        };

        auto it = map_.begin();

        while (it != map_.end()) {
            sectree *tree = it->second.get();

            tree->neighbor_list_.push_back(tree);

            metin2::log::debug("Sectree {:d}x{:d} neighbors:", tree->id_.coord.x, tree->id_.coord.y);

            int x = tree->id_.coord.x * SECTREE_SIZE;
            int y = tree->id_.coord.y * SECTREE_SIZE;

            for (uint32_t i = 0; i < 8; ++i) {
                sectree *tree2 = find(x + neighbor_coord[i].x, y + neighbor_coord[i].y);

                if (tree2) {
                    metin2::log::debug("Sectree neighbor {:d}: {:d}x{:d}", i, tree2->id_.coord.x, tree2->id_.coord.y);
                    tree->neighbor_list_.push_back(tree2);
                }
            }

            ++it;
        }
    }

    sectree_manager::sectree_manager() = default;

    sectree_manager::~sectree_manager() = default;

    auto sectree_manager::map(int32_t map_index) -> sectree_map * {
        auto it = map_sectree_.find(map_index);

        if (it == map_sectree_.end())
            return nullptr;

        return it->second.get();
    }

    auto sectree_manager::get(uint32_t index, uint32_t package) -> sectree * {
        sectree_map *sectree_map = map(index);

        if (!sectree_map)
            return nullptr;

        return sectree_map->find(package);
    }

    auto sectree_manager::get(uint32_t index, uint32_t x, uint32_t y) -> sectree * {
        sectreeid id;
        id.coord.x = x / SECTREE_SIZE;
        id.coord.y = y / SECTREE_SIZE;
        return get(index, id.package);
    }

    auto sectree_manager::load_setting_file(int32_t map_index, const char *setting_file_name, map_setting &r_setting) -> int {
        memset(&r_setting, 0, sizeof(map_setting));

        FILE *fp = fopen(setting_file_name, "r");

        if (!fp) {
            metin2::log::error("Cannot open map settings file {}", setting_file_name);
            return 0;
        }

        char buf[256], cmd[256];
        int width = 0, height = 0;

        while (fgets(buf, 256, fp)) {
            sscanf(buf, " %s ", cmd);

            if (!strcasecmp(cmd, "MapSize")) {
                sscanf(buf, " %s %d %d ", cmd, &width, &height);
            } else if (!strcasecmp(cmd, "BasePosition")) {
                sscanf(buf, " %s %d %d", cmd, &r_setting.base_x, &r_setting.base_y);
            } else if (!strcasecmp(cmd, "CellScale")) {
                sscanf(buf, " %s %d ", cmd, &r_setting.cell_scale);
            }
        }

        fclose(fp);

        if ((width == 0 && height == 0) || r_setting.cell_scale == 0) {
            metin2::log::error("Invalid map settings file {}, size or cell scale is zero", setting_file_name);
            return 0;
        }

        r_setting.index = map_index;
        r_setting.width = (r_setting.cell_scale * 128 * width);
        r_setting.height = (r_setting.cell_scale * 128 * height);
        return 1;
    }

    auto sectree_manager::build_sectree_from_setting(map_setting &r_setting) -> std::unique_ptr<sectree_map> {
        auto map_sectree = std::make_unique<sectree_map>();

        map_sectree->setting = r_setting;

        int x, y;

        for (x = r_setting.base_x; x < r_setting.base_x + r_setting.width; x += SECTREE_SIZE) {
            for (y = r_setting.base_y; y < r_setting.base_y + r_setting.height; y += SECTREE_SIZE) {
                auto tree = std::make_unique<sectree>();
                tree->id_.coord.x = x / SECTREE_SIZE;
                tree->id_.coord.y = y / SECTREE_SIZE;
                metin2::log::debug("New sectree {:d}x{:d}", tree->id_.coord.x, tree->id_.coord.y);
                map_sectree->add(tree->id_.package, std::move(tree));
            }
        }

        if ((r_setting.base_x + r_setting.width) % SECTREE_SIZE) {
            auto tree = std::make_unique<sectree>();
            tree->id_.coord.x = ((r_setting.base_x + r_setting.width) / SECTREE_SIZE) + 1;
            tree->id_.coord.y = ((r_setting.base_y + r_setting.height) / SECTREE_SIZE);
            map_sectree->add(tree->id_.package, std::move(tree));
        }

        if ((r_setting.base_y + r_setting.height) % SECTREE_SIZE) {
            auto tree = std::make_unique<sectree>();
            tree->id_.coord.x = ((r_setting.base_x + r_setting.width) / SECTREE_SIZE);
            tree->id_.coord.y = ((r_setting.base_y + r_setting.height) / SECTREE_SIZE) + 1;
            map_sectree->add(tree->id_.package, std::move(tree));
        }

        return map_sectree;
    }

    void sectree_manager::load_dungeon(int index, const char *file_name) {
        FILE *fp = fopen(file_name, "r");

        if (!fp)
            return;

        int count = 0;

        while (!feof(fp)) {
            char buf[1024];

            if (nullptr == fgets(buf, 1024, fp))
                break;

            if (buf[0] == '#' || (buf[0] == '/' && buf[1] == '/'))
                continue;

            std::istringstream ins(buf, std::ios_base::in);
            std::string position_name;
            int x, y, sx, sy, dir;

            ins >> position_name >> x >> y >> sx >> sy >> dir;

            if (ins.fail())
                continue;

            x -= sx;
            y -= sy;
            sx *= 2;
            sy *= 2;
            sx += x;
            sy += y;

            area_by_map_[index].insert(std::make_pair(position_name, area_info(x, y, sx, sy, dir)));

            count++;
        }

        fclose(fp);

        metin2::log::info("Loaded {2:d} dungeon positions for map {0:d} from {1}", index, file_name, count);
    }

    auto sectree_manager::load_map_region(const char *file_name, map_setting &r_setting, const char *map_name_value) -> bool {
        FILE *fp = fopen(file_name, "r");

        if (metin2::config::test_server())
            metin2::log::debug("Loading map region from {}", file_name);

        if (!fp)
            return false;

        int x_value = 0, y_value = 0;
        pixel_position pos[3] = {
            {0, 0, 0},
            {0, 0, 0},
            {0, 0, 0}
        };

        fscanf(fp, " %d %d ", &x_value, &y_value);

        int empire_position_count = fscanf(fp, " %d %d %d %d %d %d ",
            &pos[0].x, &pos[0].y,
            &pos[1].x, &pos[1].y,
            &pos[2].x, &pos[2].y);

        fclose(fp);

        if (empire_position_count == 6) {
            for (int n = 0; n < 3; ++n)
                metin2::log::debug("Map region: empire start position {:d} {:d}", pos[n].x, pos[n].y);
        } else {
            metin2::log::debug("Map region: no empire specific start position");
        }

        map_region region;

        region.index = r_setting.index;
        region.sx = r_setting.base_x;
        region.sy = r_setting.base_y;
        region.ex = r_setting.base_x + r_setting.width;
        region.ey = r_setting.base_y + r_setting.height;

        region.map_name = map_name_value;

        region.pos_spawn.x = r_setting.base_x + (x_value * 100);
        region.pos_spawn.y = r_setting.base_y + (y_value * 100);

        r_setting.pos_spawn = region.pos_spawn;

        metin2::log::info("Loaded map region {:d}, x {:d} - {:d}, y {:d} - {:d}, town at {:d} {:d}",
            region.index,
            region.sx,
            region.ex,
            region.sy,
            region.ey,
            region.pos_spawn.x,
            region.pos_spawn.y);

        if (empire_position_count == 6) {
            region.empire_spawn_different = true;

            for (int i = 0; i < 3; i++) {
                region.pos_empire[i].x = r_setting.base_x + (pos[i].x * 100);
                region.pos_empire[i].y = r_setting.base_y + (pos[i].y * 100);
            }
        } else {
            region.empire_spawn_different = false;
        }

        vec_map_region_.push_back(region);

        return true;
    }

    auto sectree_manager::load_attribute(sectree_map *map_sectree, const char *file_name, map_setting &r_setting) -> bool {
        FILE *fp = fopen(file_name, "rb");

        if (!fp) {
            metin2::log::error("Cannot open map attribute file {}", file_name);
            return false;
        }

        int width, height;

        fread(&width, sizeof(int), 1, fp);
        fread(&height, sizeof(int), 1, fp);

        int max_mem_size = get_lzo_manager().max_compressed_size(sizeof(uint32_t) * (SECTREE_SIZE / CELL_SIZE) * (SECTREE_SIZE / CELL_SIZE));

        unsigned int size_value;
        lzo_uint dest_size;

        std::vector<uint8_t> ab_comp(max_mem_size);
        auto attr = std::make_unique_for_overwrite<uint32_t[]>(max_mem_size);

        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x) {
                sectreeid id;
                id.coord.x = (r_setting.base_x / SECTREE_SIZE) + x;
                id.coord.y = (r_setting.base_y / SECTREE_SIZE) + y;

                sectree *tree = map_sectree->find(id.package);

                if (tree == nullptr) {
                    metin2::log::error("Map attribute file {} refers to a missing sectree (package {:x}, coord ({:d}, {:d}), map {:d}, base ({:d}, {:d}))",
                        file_name, id.package, id.coord.x, id.coord.y, r_setting.index, r_setting.base_x, r_setting.base_y);
                    metin2::log::error("Attribute position ({:d}, {:d}), attribute size ({:d}, {:d})", x, y, width, height);
                    metin2::log::error("Check that the map size in Setting.txt matches server_attr");

                    map_sectree->dump_all_to_sys_err();
                    abort();

                    return false;
                }

                if (tree->id_.package != id.package) {
                    metin2::log::error("Sectree id mismatch, got {:d}, requested {:d}",
                        tree->id_.package, id.package);
                    fclose(fp);

                    return false;
                }

                fread(&size_value, sizeof(int), 1, fp);
                if (size_value > ab_comp.size()) {
                    metin2::log::error("Attribute block of {:d} bytes is larger than {:d}", size_value, ab_comp.size());
                    fclose(fp);
                    return false;
                }
                fread(ab_comp.data(), sizeof(char), size_value, fp);

                dest_size = sizeof(uint32_t) * max_mem_size;
                get_lzo_manager().decompress(ab_comp.data(), size_value, (uint8_t *)attr.get(), &dest_size);

                if (dest_size != sizeof(uint32_t) * (SECTREE_SIZE / CELL_SIZE) * (SECTREE_SIZE / CELL_SIZE)) {
                    metin2::log::error("Map attribute file {}: sectree {:d}x{:d} decompressed to wrong size {:d}",
                        file_name, tree->id_.coord.x, tree->id_.coord.y, static_cast<int>(dest_size));
                    fclose(fp);

                    return false;
                }

                tree->bind_attribute(std::make_unique<attribute>(attr.get(), SECTREE_SIZE / CELL_SIZE, SECTREE_SIZE / CELL_SIZE));
            }

        fclose(fp);

        return true;
    }

    namespace {
        auto base_map_index(int32_t map_index) -> int32_t {
            return map_index >= 10000 ? map_index / 10000 : map_index;
        }

        auto region_contains(const map_region &region, int32_t x, int32_t y) -> bool {
            return x >= region.sx && y >= region.sy && x < region.ex && y < region.ey;
        }
    }

    auto sectree_manager::recall_position_by_empire(int map_index, uint8_t empire, pixel_position &r_pos) -> bool {
        const map_region *region = get_map_region(base_map_index(map_index));
        if (!region)
            return false;

        if (region->empire_spawn_different && empire >= 1 && empire <= 3)
            r_pos = region->pos_empire[empire - 1];
        else
            r_pos = region->pos_spawn;

        return true;
    }

    auto sectree_manager::center_position_of_map(int32_t map_index, pixel_position &r_pos) -> bool {
        const map_region *region = get_map_region(map_index);
        if (!region)
            return false;

        r_pos.x = region->sx + (region->ex - region->sx) / 2;
        r_pos.y = region->sy + (region->ey - region->sy) / 2;
        r_pos.z = 0;
        return true;
    }

    auto sectree_manager::map_base_position_by_map_index(int32_t map_index, pixel_position &r_pos) -> bool {
        map_index = base_map_index(map_index);

        const map_region *region = get_map_region(map_index);
        if (!region)
            return false;

        r_pos.x = region->sx;
        r_pos.y = region->sy;
        r_pos.z = 0;
        return true;
    }

    auto sectree_manager::map_base_position(int32_t x, int32_t y, pixel_position &r_pos) -> bool {
        auto it = std::ranges::find_if(vec_map_region_, [&](const map_region &region) -> bool { return region_contains(region, x, y); });
        if (it == vec_map_region_.end())
            return false;

        r_pos.x = it->sx;
        r_pos.y = it->sy;
        r_pos.z = 0;
        return true;
    }

    auto sectree_manager::find_region_by_partial_name(const char *map_name_value) -> const map_region * {
        auto it = std::ranges::find_if(vec_map_region_, [&](const map_region &region) -> bool { return region.map_name.contains(map_name_value); });
        return it != vec_map_region_.end() ? &*it : nullptr;
    }

    auto sectree_manager::get_map_region(int32_t map_index) -> const map_region * {
        auto it = std::ranges::find(vec_map_region_, map_index, &map_region::index);
        return it != vec_map_region_.end() ? &*it : nullptr;
    }

    auto sectree_manager::map_index(int32_t x, int32_t y) -> int {
        auto it = std::ranges::find_if(vec_map_region_, [&](const map_region &region) -> bool { return region_contains(region, x, y); });
        if (it != vec_map_region_.end())
            return it->index;

        metin2::log::warn("No map region contains position ({:d}, {:d})", x, y);

        for (const map_region &region : vec_map_region_)
            metin2::log::debug("Map region {:d}: ({:d}, {:d}) - ({:d}, {:d})", region.index, region.sx, region.sy, region.ex, region.ey);

        return 0;
    }

    auto sectree_manager::build(const char *list_file_name, const char *map_base_path) -> int {
        if (true == metin2::config::test_server()) {
            metin2::log::debug("Building maps from list {} in {}", list_file_name, map_base_path);
        }

        FILE *fp = fopen(list_file_name, "r");

        if (nullptr == fp)
            return 0;

        char buf[256 + 1];
        std::string filename;
        char map_name[256];
        int index_value;

        while (fgets(buf, 256, fp)) {
            buf[strcspn(buf, "\r\n")] = '\0';

            if (!strncmp(buf, "//", 2) || *buf == '#')
                continue;

            sscanf(buf, " %d %s ", &index_value, map_name);

            filename = std::format("{}/{}/Setting.txt", map_base_path, map_name);

            map_setting setting;
            setting.index = index_value;

            if (!load_setting_file(index_value, filename.c_str(), setting)) {
                metin2::log::error("Cannot load map settings file {}", filename);
                fclose(fp);
                return 0;
            }

            filename = std::format("{}/{}/Town.txt", map_base_path, map_name);

            if (!load_map_region(filename.c_str(), setting, map_name)) {
                metin2::log::error("Cannot load map region file {}", filename);
                fclose(fp);
                return 0;
            }

            if (true == metin2::config::test_server())
                metin2::log::debug("Building map {}{} (index {:d})", map_base_path, map_name, index_value);

            if (map_allow_find(index_value)) {
                std::unique_ptr<sectree_map> new_map_sectree = build_sectree_from_setting(setting);
                sectree_map *map_sectree = new_map_sectree.get();
                map_sectree_.try_emplace(index_value, std::move(new_map_sectree));

                filename = std::format("{}/{}/server_attr", map_base_path, map_name);
                load_attribute(map_sectree, filename.c_str(), setting);

                filename = std::format("{}/{}/regen.txt", map_base_path, map_name);
                regen_load(filename.c_str(), setting.index, setting.base_x, setting.base_y);

                filename = std::format("{}/{}/npc.txt", map_base_path, map_name);
                regen_load(filename.c_str(), setting.index, setting.base_x, setting.base_y);

                filename = std::format("{}/{}/boss.txt", map_base_path, map_name);
                regen_load(filename.c_str(), setting.index, setting.base_x, setting.base_y);

                filename = std::format("{}/{}/stone.txt", map_base_path, map_name);
                regen_load(filename.c_str(), setting.index, setting.base_x, setting.base_y);

                filename = std::format("{}/{}/dungeon.txt", map_base_path, map_name);
                load_dungeon(index_value, filename.c_str());

                map_sectree->build();
            }
        }

        fclose(fp);

        return 1;
    }

    auto sectree_manager::is_movable_position(int32_t map_index, int32_t x, int32_t y) -> bool {
        sectree *tree = get_sectree_manager().get(map_index, x, y);

        if (!tree)
            return false;

        return (!tree->is_attr(x, y, ATTR_BLOCK | ATTR_OBJECT));
    }

    auto sectree_manager::movable_position(int32_t map_index, int32_t x, int32_t y, pixel_position &pos) -> bool {
        int i = 0;

        do {
            int32_t dx = x + arround_coords[i].x;
            int32_t dy = y + arround_coords[i].y;

            sectree *tree = get_sectree_manager().get(map_index, dx, dy);

            if (!tree)
                continue;

            if (!tree->is_attr(dx, dy, ATTR_BLOCK | ATTR_OBJECT)) {
                pos.x = dx;
                pos.y = dy;
                return true;
            }
        } while (++i < ARROUND_COORD_MAX_NUM);

        pos.x = x;
        pos.y = y;
        return false;
    }

    auto sectree_manager::valid_location(int32_t map_index, int32_t x, int32_t y, int32_t &valid_map_index, pixel_position &r_pos, uint8_t empire) -> bool {
        sectree_map *sectree_map = map(map_index);

        if (!sectree_map) {
            if (map_index >= 10000) {
                return valid_location(map_index / 10000, x, y, valid_map_index, r_pos);
            } else {
                metin2::log::error("No sectree map for map {:d}", map_index);
                return false;
            }
        }

        const int32_t real_map_index = base_map_index(map_index);

        if (get_map_region(real_map_index)) {
            if (!sectree_map->find(x, y)) {
                metin2::log::error("No sectree at {:d} {:d} on map {:d}", x, y, map_index);
                return false;
            }

            valid_map_index = map_index;
            r_pos.x = x;
            r_pos.y = y;
            return true;
        }

        metin2::log::error("Invalid location {1:d}x{2:d} on map {0:d}", real_map_index, x, y);
        return false;
    }

    auto sectree_manager::random_location(int32_t map_index, pixel_position &r_pos, uint32_t current_x, uint32_t current_y, int max_distance) -> bool {
        sectree_map *sectree_map = map(map_index);

        if (!sectree_map)
            return false;

        uint32_t x, y;

        for (const map_region &region : vec_map_region_) {
            if (region.index != map_index)
                continue;

            int i = 0;

            while (i++ < 100) {
                x = number(region.sx + 50, region.ex - 50);
                y = number(region.sy + 50, region.ey - 50);

                if (max_distance != 0) {
                    int d;

                    d = std::abs((float)current_x - x);

                    if (d > max_distance) {
                        if (x < current_x)
                            x = current_x - max_distance;
                        else
                            x = current_x + max_distance;
                    }

                    d = std::abs((float)current_y - y);

                    if (d > max_distance) {
                        if (y < current_y)
                            y = current_y - max_distance;
                        else
                            y = current_y + max_distance;
                    }
                }

                sectree *tree = sectree_map->find(x, y);

                if (!tree)
                    continue;

                if (tree->is_attr(x, y, ATTR_BLOCK | ATTR_OBJECT))
                    continue;

                r_pos.x = x;
                r_pos.y = y;
                return true;
            }
        }

        return false;
    }

    auto sectree_manager::create_private_map(int32_t map_index) -> int32_t {
        if (map_index >= 10000)
            return 0;

        sectree_map *map_sectree = map(map_index);

        if (!map_sectree) {
            metin2::log::error("Cannot create private map, map {:d} not found", map_index);
            return 0;
        }

        int32_t base = map_index * 10000;
        int index_cap = 10000;
        if (map_index == 107 || map_index == 108 || map_index == 109) {
            index_cap = (metin2::config::test_server() ? 1 : 51);
        }
        auto it = next_private_index_map_.find(map_index);
        if (it == next_private_index_map_.end()) {
            it = next_private_index_map_.insert(private_index_map_type::value_type(map_index, 0)).first;
        }
        int i, next_index = it->second;
        for (i = 0; i < index_cap; ++i) {
            if (map(base + next_index) == nullptr) {
                break;
            }
            if (++next_index >= index_cap) {
                next_index = 0;
            }
        }
        if (i == index_cap) {
            return 0;
        }
        int32_t new_map_index = base + next_index;
        if (++next_index >= index_cap) {
            next_index = 0;
        }
        it->second = next_index;

        map_sectree_.try_emplace(new_map_index, std::make_unique<sectree_map>(*map_sectree));

        metin2::log::info("Private map {:d} created from map {:d}", new_map_index, map_index);
        return new_map_index;
    }

    void sectree_manager::destroy_private_map(int32_t map_index) {
        if (map_index < 10000)
            return;

        sectree_map *map_sectree = map(map_index);

        if (!map_sectree)
            return;

        // Entities on this map but outside every sectree cannot be deleted here.
        map_sectree->for_each([](entity *ent) {
            if (ent->is_type(ENTITY_CHARACTER)) {
                auto *ch = (character *)ent;
                metin2::log::info("Private map: removing character {}", ch->name());

                if (ch->get_desc())
                    get_desc_manager().destroy_desc(ch->get_desc());
                else
                    get_character_manager().destroy_character(ch);
            } else if (ent->is_type(ENTITY_ITEM)) {
                item *item = (game::items::item *)ent;
                metin2::log::info("Private map: removing item {}", item->name());

                get_item_manager().destroy_item(item);
            } else
                metin2::log::error("Private map: cannot remove entity of unknown type {:d}", ent->type());
        });

        map_sectree_.erase(map_index);

        metin2::log::info("Private map {:d} destroyed", map_index);
    }

    auto sectree_manager::dungeon_area(int32_t map_index) -> area_map & {
        auto it = area_by_map_.find(map_index);

        if (it == area_by_map_.end()) {
            return area_by_map_[-1];
        }
        return it->second;
    }

    void sectree_manager::send_npc_position(character *ch) {
        desc *d = ch->get_desc();
        if (!d)
            return;

        int32_t map_index = ch->map_index();

        if (npc_position_[map_index].empty())
            return;

        temp_buffer buf;
        packet_gcnpc_position p;
        p.header = HEADER_GC_NPC_POSITION;
        p.count = npc_position_[map_index].size();

        npc_position np;

        decltype((npc_position_[map_index]).begin()) it;

        for (it = npc_position_[map_index].begin(); it != npc_position_[map_index].end(); ++it) {
            np.type = it->type;
            strlcpy(np.name, it->name, sizeof(np.name));
            np.x = it->x;
            np.y = it->y;
            buf.write(&np, sizeof(np));
        }

        p.size = sizeof(p) + buf.size();

        if (buf.size()) {
            d->buffered_packet(&p, sizeof(packet_gcnpc_position));
            d->packet(buf.read_peek(), buf.size());
        } else
            d->packet(&p, sizeof(packet_gcnpc_position));
    }

    void sectree_manager::insert_npc_position(int32_t map_index, uint8_t type, const char *name, int32_t x, int32_t y) {
        npc_position_[map_index].push_back(npc_info(type, name, x, y));
    }

    auto sectree_manager::empire_from_map_index(int32_t map_index) -> uint8_t {
        if (map_index >= 1 && map_index <= 20)
            return 1;

        if (map_index >= 21 && map_index <= 40)
            return 2;

        if (map_index >= 41 && map_index <= 60)
            return 3;

        if (map_index == 184 || map_index == 185)
            return 1;

        if (map_index == 186 || map_index == 187)
            return 2;

        if (map_index == 188 || map_index == 189)
            return 3;

        switch (map_index) {
        case 190:
            return 1;
        case 191:
            return 2;
        case 192:
            return 3;
        }

        return 0;
    }

    auto sectree_manager::for_attr_region_cell(int32_t map_index, int32_t cx, int32_t cy, uint32_t attr, attr_region_mode mode) -> bool {
        sectreeid id;

        id.coord.x = cx / (SECTREE_SIZE / CELL_SIZE);
        id.coord.y = cy / (SECTREE_SIZE / CELL_SIZE);

        int32_t tree_cx = id.coord.x * (SECTREE_SIZE / CELL_SIZE);
        int32_t tree_cy = id.coord.y * (SECTREE_SIZE / CELL_SIZE);

        sectree *sec = get(map_index, id.package);
        if (!sec)
            return false;

        switch (mode) {
        case ATTR_REGION_MODE_SET:
            sec->set_attribute(cx - tree_cx, cy - tree_cy, attr);
            break;

        case ATTR_REGION_MODE_REMOVE:
            sec->remove_attribute(cx - tree_cx, cy - tree_cy, attr);
            break;

        case ATTR_REGION_MODE_CHECK:
            if (sec->is_attr(cx * CELL_SIZE, cy * CELL_SIZE, ATTR_OBJECT))
                return true;
            break;

        default:
            metin2::log::error("Unknown attribute region mode {:d}", mode);
            break;
        }

        return false;
    }

    auto sectree_manager::for_attr_region_right_angle(int32_t map_index, int32_t cx, int32_t cy, int32_t cw, int32_t ch, int32_t rotate, uint32_t attr, attr_region_mode mode) -> bool {
        if (1 == rotate / 90 || 3 == rotate / 90) {
            for (int x = 0; x < ch; ++x)
                for (int y = 0; y < cw; ++y) {
                    if (for_attr_region_cell(map_index, cx + x, cy + y, attr, mode))
                        return true;
                }
        }
        if (0 == rotate / 90 || 2 == rotate / 90) {
            for (int x = 0; x < cw; ++x)
                for (int y = 0; y < ch; ++y) {
                    if (for_attr_region_cell(map_index, cx + x, cy + y, attr, mode))
                        return true;
                }
        }

        return mode == ATTR_REGION_MODE_CHECK ? false : true;
    }

    auto sectree_manager::for_attr_region_free_angle(int32_t map_index, int32_t cx, int32_t cy, int32_t cw, int32_t ch, int32_t rotate, uint32_t attr, attr_region_mode mode) -> bool {
        float fx1 = (-cw / 2) * sinf(float(rotate) / 180.0f * 3.14f) + (-ch / 2) * cosf(float(rotate) / 180.0f * 3.14f);
        float fy1 = (-cw / 2) * cosf(float(rotate) / 180.0f * 3.14f) - (-ch / 2) * sinf(float(rotate) / 180.0f * 3.14f);

        float fx2 = (+cw / 2) * sinf(float(rotate) / 180.0f * 3.14f) + (-ch / 2) * cosf(float(rotate) / 180.0f * 3.14f);
        float fy2 = (+cw / 2) * cosf(float(rotate) / 180.0f * 3.14f) - (-ch / 2) * sinf(float(rotate) / 180.0f * 3.14f);

        float fx3 = (-cw / 2) * sinf(float(rotate) / 180.0f * 3.14f) + (+ch / 2) * cosf(float(rotate) / 180.0f * 3.14f);
        float fy3 = (-cw / 2) * cosf(float(rotate) / 180.0f * 3.14f) - (+ch / 2) * sinf(float(rotate) / 180.0f * 3.14f);

        float fx4 = (+cw / 2) * sinf(float(rotate) / 180.0f * 3.14f) + (+ch / 2) * cosf(float(rotate) / 180.0f * 3.14f);
        float fy4 = (+cw / 2) * cosf(float(rotate) / 180.0f * 3.14f) - (+ch / 2) * sinf(float(rotate) / 180.0f * 3.14f);

        float fdx1 = fx2 - fx1;
        float fdy1 = fy2 - fy1;
        float fdx2 = fx1 - fx3;
        float fdy2 = fy1 - fy3;

        if (0 == fdx1 || 0 == fdx2) {
            metin2::log::error("Attribute region on map {:d} has a zero-width edge", map_index);
            return false;
        }

        float tilt1 = float(fdy1) / float(fdx1);
        float tilt2 = float(fdy2) / float(fdx2);
        float fb1 = fy1 - tilt1 * fx1;
        float fb2 = fy1 - tilt2 * fx1;
        float fb3 = fy4 - tilt1 * fx4;
        float fb4 = fy4 - tilt2 * fx4;

        float fx_min = std::min({fx1, fx2, fx3, fx4});
        float fx_max = std::max({fx1, fx2, fx3, fx4});
        for (int i = int(fx_min); i < int(fx_max); ++i) {
            float fy_value1 = tilt1 * i + std::min(fb1, fb3);
            float fy_value2 = tilt2 * i + std::min(fb2, fb4);

            float fy_value3 = tilt1 * i + std::max(fb1, fb3);
            float fy_value4 = tilt2 * i + std::max(fb2, fb4);

            float min_value;
            float max_value;
            if (abs(int(fy_value1)) < abs(int(fy_value2)))
                max_value = fy_value1;
            else
                max_value = fy_value2;
            if (abs(int(fy_value3)) < abs(int(fy_value4)))
                min_value = fy_value3;
            else
                min_value = fy_value4;

            for (int j = int(std::min(min_value, max_value)); j < int(std::max(min_value, max_value)); ++j) {
                if (for_attr_region_cell(map_index, cx + (cw / 2) + i, cy + (ch / 2) + j, attr, mode))
                    return true;
            }
        }

        return mode == ATTR_REGION_MODE_CHECK ? false : true;
    }

    auto sectree_manager::for_attr_region(int32_t map_index, int32_t start_x, int32_t start_y, int32_t end_x, int32_t end_y, int32_t rotate, uint32_t attr, attr_region_mode mode) -> bool {
        sectree_map *map_sectree = map(map_index);

        if (!map_sectree) {
            metin2::log::error("No sectree map for map {:d}", map_index);
            return mode == ATTR_REGION_MODE_CHECK ? true : false;
        }

        start_x -= start_x % CELL_SIZE;
        start_y -= start_y % CELL_SIZE;
        end_x += end_x % CELL_SIZE;
        end_y += end_y % CELL_SIZE;

        int32_t cx = start_x / CELL_SIZE;
        int32_t cy = start_y / CELL_SIZE;
        int32_t cw = (end_x - start_x) / CELL_SIZE;
        int32_t ch = (end_y - start_y) / CELL_SIZE;

        metin2::log::debug("Attribute region ({:d}, {:d}) - ({:d}, {:d})", start_x, start_y, end_x, end_y);

        rotate = rotate % 360;

        if (0 == rotate % 90)
            return for_attr_region_right_angle(map_index, cx, cy, cw, ch, rotate, attr, mode);

        return for_attr_region_free_angle(map_index, cx, cy, cw, ch, rotate, attr, mode);
    }

    auto sectree_manager::save_attribute_to_image(int map_index, const char *file_name, sectree_map *map_src) -> bool {
        sectree_map *map_value = sectree_manager::map(map_index);

        if (!map_value) {
            if (map_src)
                map_value = map_src;
            else {
                metin2::log::error("No sectree map for map {:d}", map_index);
                return false;
            }
        }

        int map_height = map_value->setting.height / 128 / 200;
        int map_width = map_value->setting.width / 128 / 200;

        if (map_height < 0 || map_width < 0) {
            metin2::log::error("Invalid map size {:d}x{:d}", map_width, map_height);
            return false;
        }

        metin2::log::info("Saving map attributes ({:d}x{:d}) to {}", map_width, map_height, file_name);

        const int image_width = 512 * map_width;
        const int image_height = 512 * map_height;
        std::vector<uint32_t> image(static_cast<size_t>(image_width) * image_height);
        uint32_t *dest = image.data();

        int pixels = 0;
        int x, x2;
        int y, y2;

        auto line = std::make_unique_for_overwrite<uint32_t[]>(SECTREE_SIZE / CELL_SIZE);

        for (y = 0; y < 4 * map_height; ++y) {
            for (y2 = 0; y2 < SECTREE_SIZE / CELL_SIZE; ++y2) {
                for (x = 0; x < 4 * map_width; ++x) {
                    sectreeid id;

                    id.coord.x = x + map_value->setting.base_x / SECTREE_SIZE;
                    id.coord.y = y + map_value->setting.base_y / SECTREE_SIZE;

                    sectree *sec = map_value->find(id.package);

                    if (!sec) {
                        metin2::log::error("No sectree at {:d}x{:d} (map base {:d}, {:d})", id.coord.x, id.coord.y, map_value->setting.base_x, map_value->setting.base_y);
                        continue;
                    }

                    sec->attribute_->copy_row(y2, line.get());

                    if (!line) {
                        metin2::log::error("Sectree has no attribute data");
                        continue;
                    }

                    for (x2 = 0; x2 < SECTREE_SIZE / CELL_SIZE; ++x2) {
                        uint32_t color;

                        if (is_set(line[x2], ATTR_WATER))
                            color = 0xff0000ff;
                        else if (is_set(line[x2], ATTR_BANPK))
                            color = 0xff00ff00;
                        else if (is_set(line[x2], ATTR_BLOCK))
                            color = 0xffff0000;
                        else
                            color = 0xffffffff;

                        *(dest++) = color;
                        pixels++;
                    }
                }
            }
        }

        if (save_tga_bgra(file_name, image.data(), image_width, image_height)) {
            metin2::log::info("Map {:d} attributes saved to {} ({:d} bytes)", map_index, file_name, pixels);
            return true;
        } else {
            metin2::log::error("Cannot save map {:d} attributes to {}", map_index, file_name);
            return false;
        }
    }

    void sectree_manager::purge_monsters_in_map(int32_t map_index) {
        sectree_map *sectree = get_sectree_manager().map(map_index);

        if (sectree != nullptr) {
            sectree->for_each([](entity *ent) {
                if (ent->is_type(ENTITY_CHARACTER) == true) {
                    auto *ch = (character *)ent;

                    if (ch->is_monster() == true && !ch->is_pet()) {
                        get_character_manager().destroy_character(ch);
                    }
                }
            });
        }
    }

    void sectree_manager::purge_stones_in_map(int32_t map_index) {
        sectree_map *sectree = get_sectree_manager().map(map_index);

        if (sectree != nullptr) {
            sectree->for_each([](entity *ent) {
                if (ent->is_type(ENTITY_CHARACTER) == true) {
                    auto *ch = (character *)ent;

                    if (ch->is_stone() == true) {
                        get_character_manager().destroy_character(ch);
                    }
                }
            });
        }
    }

    auto sectree_manager::monster_count_in_map(int32_t map_index) -> size_t {
        sectree_map *sectree = get_sectree_manager().map(map_index);

        if (sectree != nullptr) {
            size_t count = 0;

            sectree->for_each([&count](entity *ent) {
                if (ent->is_type(ENTITY_CHARACTER) == true && ((character *)ent)->is_monster() == true)
                    ++count;
            });

            return count;
        }

        return 0;
    }

    auto sectree_manager::monster_count_in_map(int32_t map_index, uint32_t vnum) -> size_t {
        sectree_map *sectree = get_sectree_manager().map(map_index);

        if (nullptr != sectree) {
            size_t count = 0;

            sectree->for_each([vnum, &count](entity *ent) {
                if (true == ent->is_type(ENTITY_CHARACTER)) {
                    auto *ch = static_cast<character *>(ent);

                    if (true == ch->is_stone() && ch->get_mob_table().vnum == vnum)
                        count++;
                }
            });

            return count;
        }

        return 0;
    }
}
