#include <metin2/config/config.h>
#include <metin2/game/world/regen.h>

#include <metin2/game/app/config.h>
#include <metin2/game/app/event.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/characters/mob_manager.h>
#include <metin2/game/world/dungeon.h>
#include <metin2/game/world/sectree_manager.h>

#include <metin2/common/utils.h>
#include <metin2/core/utils.h>
#include <metin2/log/log.h>
#include <metin2/proto/character_types.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace metin2::game::inline world {

    namespace {
        std::vector<std::unique_ptr<regen>> regen_list;
    }

    enum regen_modes {
        MODE_TYPE = 0,
        MODE_SX,
        MODE_SY,
        MODE_EX,
        MODE_EY,
        MODE_Z_SECTION,
        MODE_DIRECTION,
        MODE_REGEN_TIME,
        MODE_REGEN_PERCENT,
        MODE_MAX_COUNT,
        MODE_VNUM
    };

    static auto is_space(int c) -> bool {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }

    static auto get_word(std::istream &in, std::string &word) -> bool {
        word.clear();
        bool quoted = false;

        for (int c; (c = in.get()) != std::char_traits<char>::eof();) {
            if (word.empty()) {
                if (c == '"') {
                    quoted = true;
                    continue;
                }

                if (is_space(c))
                    continue;
            }

            if (quoted ? c == '"' : is_space(c))
                return true;

            word += static_cast<char>(c);

            if (word == "//")
                return true;
        }

        return !word.empty();
    }

    static void next_line(std::istream &in) {
        in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }

    static auto read_line(std::istream &in, regen *regen) -> bool {
        std::string word;
        int mode = MODE_TYPE;

        while (get_word(in, word)) {
            if (word.starts_with("//")) {
                next_line(in);
                continue;
            }

            const char *tmp = word.c_str();

            switch (mode) {
            case MODE_TYPE:
                if (tmp[0] == 'm')
                    regen->type = REGEN_TYPE_MOB;
                else if (tmp[0] == 'g') {
                    regen->type = REGEN_TYPE_GROUP;

                    if (tmp[1] == 'a')
                        regen->is_aggressive = true;
                } else if (tmp[0] == 'e')
                    regen->type = REGEN_TYPE_EXCEPTION;
                else if (tmp[0] == 'r')
                    regen->type = REGEN_TYPE_GROUP_GROUP;
                else if (tmp[0] == 's')
                    regen->type = REGEN_TYPE_ANYWHERE;
                else {
                    metin2::log::error("Unknown regen type '{:c}'", tmp[0]);
                    exit(1);
                }

                ++mode;
                break;

            case MODE_SX:
                str_to_number(regen->sx, tmp);
                ++mode;
                break;

            case MODE_SY:
                str_to_number(regen->sy, tmp);
                ++mode;
                break;

            case MODE_EX:
                {
                    int x = 0;
                    str_to_number(x, tmp);

                    regen->sx -= x;
                    regen->ex = regen->sx + x * 2;

                    regen->sx *= 100;
                    regen->ex *= 100;

                    ++mode;
                }
                break;

            case MODE_EY:
                {
                    int y = 0;
                    str_to_number(y, tmp);

                    regen->sy -= y;
                    regen->ey = regen->sy + y * 2;

                    regen->sy *= 100;
                    regen->ey *= 100;

                    ++mode;
                }
                break;

            case MODE_Z_SECTION:
                str_to_number(regen->z_section, tmp);

                if (regen->type == REGEN_TYPE_EXCEPTION)
                    return true;

                ++mode;
                break;

            case MODE_DIRECTION:
                str_to_number(regen->direction, tmp);
                ++mode;
                break;

            case MODE_REGEN_TIME:
                {
                    regen->time = 0;
                    int tmp_time = 0;

                    for (const char c : word) {
                        switch (c) {
                        case 'h':
                            regen->time += tmp_time * 3600;
                            tmp_time = 0;
                            break;

                        case 'm':
                            regen->time += tmp_time * 60;
                            tmp_time = 0;
                            break;

                        case 's':
                            regen->time += tmp_time;
                            tmp_time = 0;
                            break;

                        default:
                            if (c >= '0' && c <= '9') {
                                tmp_time *= 10;
                                tmp_time += (c - '0');
                            }
                        }
                    }

                    ++mode;
                }
                break;

            case MODE_REGEN_PERCENT:
                ++mode;
                break;

            case MODE_MAX_COUNT:
                regen->count = 0;
                str_to_number(regen->max_count, tmp);
                ++mode;
                break;

            case MODE_VNUM:
                str_to_number(regen->vnum, tmp);
                ++mode;
                return true;
            }
        }

        return false;
    }

    static void regen_spawn(regen *regen, bool once, dungeon *dungeon_value = nullptr) {
        const uint32_t num = regen->max_count - regen->count;
        auto &chars = get_character_manager();

        for (uint32_t i = 0; i < num; ++i) {
            character *ch = nullptr;

            if (regen->type == REGEN_TYPE_ANYWHERE) {
                ch = chars.spawn_mob_random_position(regen->vnum, regen->map_index);
            } else if (regen->sx == regen->ex && regen->sy == regen->ey) {
                ch = chars.spawn_mob(regen->vnum,
                    regen->map_index,
                    regen->sx,
                    regen->sy,
                    regen->z_section,
                    false,
                    regen->direction == 0 ? number(0, 7) * 45 : (regen->direction - 1) * 45);
            } else if (regen->type == REGEN_TYPE_MOB) {
                if (dungeon_value)
                    ch = chars.spawn_mob_range(regen->vnum, regen->map_index, regen->sx, regen->sy, regen->ex, regen->ey, true);
                else
                    ch = chars.spawn_mob_range(regen->vnum, regen->map_index, regen->sx, regen->sy, regen->ex, regen->ey, true, regen->is_aggressive, regen->is_aggressive);
            } else if (regen->type == REGEN_TYPE_GROUP) {
                if (chars.spawn_group(regen->vnum, regen->map_index, regen->sx, regen->sy, regen->ex, regen->ey, once ? nullptr : regen, regen->is_aggressive, dungeon_value))
                    ++regen->count;
            } else if (regen->type == REGEN_TYPE_GROUP_GROUP) {
                if (chars.spawn_group_group(regen->vnum, regen->map_index, regen->sx, regen->sy, regen->ex, regen->ey, once ? nullptr : regen, regen->is_aggressive, dungeon_value))
                    ++regen->count;
            }

            if (ch) {
                ++regen->count;

                if (dungeon_value)
                    ch->set_dungeon(dungeon_value);

                if (!once)
                    ch->set_regen(regen);
            }
        }
    }

    static auto find_regen_mob(const regen &entry) -> const mob * {
        const mob *p = get_mob_manager().get(entry.vnum);

        if (!p)
            metin2::log::error("Regen refers to unknown mob vnum {:d}", entry.vnum);

        return p;
    }

    // Monsters cannot stand on blocked cells; an area without a single free cell never spawns anything.
    static auto has_free_cell(int32_t map_index, const regen &entry) -> bool {
        if (entry.type == REGEN_TYPE_MOB) {
            const mob *proto = get_mob_manager().get(entry.vnum);
            if (!proto || proto->table.type == CHAR_TYPE_NPC || proto->table.type == CHAR_TYPE_WARP || proto->table.type == CHAR_TYPE_GOTO)
                return true;
        }

        for (int32_t y = entry.sy; y <= entry.ey; y += CELL_SIZE) {
            for (int32_t x = entry.sx; x <= entry.ex; x += CELL_SIZE) {
                sectree *tree = get_sectree_manager().get(map_index, x, y);
                if (tree && !is_set(tree->get_attribute(x, y), ATTR_BLOCK | ATTR_OBJECT))
                    return true;
            }
        }
        return false;
    }

    template <typename F>
    static auto load_regen_file(const char *filename, int32_t map_index_value, int base_x, int base_y, bool missing_is_error, F &&on_entry) -> bool {
        std::ifstream in(filename);

        if (!in) {
            if (missing_is_error)
                metin2::log::error("Regen file {} not found", filename);
            else
                metin2::log::info("Regen file {} not found, skipping", filename);
            return false;
        }

        int blocked_areas = 0;

        while (true) {
            regen entry;

            if (!read_line(in, &entry))
                break;

            if (entry.type == REGEN_TYPE_EXCEPTION)
                continue;

            entry.map_index = map_index_value;
            entry.count = 0;
            std::tie(entry.sx, entry.ex) = std::minmax({entry.sx + base_x, entry.ex + base_x});
            std::tie(entry.sy, entry.ey) = std::minmax({entry.sy + base_y, entry.ey + base_y});

            // Spawn areas may reach past the map's edge; only the part on the map can be used.
            if (const sectree_map *map = get_sectree_manager().map(map_index_value)) {
                const auto &setting = map->setting;
                entry.sx = std::max(entry.sx, setting.base_x);
                entry.sy = std::max(entry.sy, setting.base_y);
                entry.ex = std::min(entry.ex, setting.base_x + setting.width - 1);
                entry.ey = std::min(entry.ey, setting.base_y + setting.height - 1);

                if (entry.sx > entry.ex || entry.sy > entry.ey) {
                    metin2::log::warn("Regen {}: area of vnum {:d} lies outside map {:d}, skipped", filename, entry.vnum, map_index_value);
                    continue;
                }

                if (entry.type != REGEN_TYPE_ANYWHERE && !has_free_cell(map_index_value, entry)) {
                    metin2::log::debug("Regen {}: area of vnum {:d} at {:d},{:d} is entirely blocked", filename, entry.vnum, entry.sx, entry.sy);
                    ++blocked_areas;
                    continue;
                }
            }

            on_entry(entry);
        }

        if (blocked_areas > 0)
            metin2::log::warn("Regen {}: {:d} spawn areas lie entirely on blocked cells and are skipped", filename, blocked_areas);

        return true;
    }

    EVENTFUNC(dungeon_regen_event) {
        auto *info = dynamic_cast<dungeon_regen_event_info *>(event->info.get());

        if (info == nullptr) {
            metin2::log::error("Dungeon regen event has no event info");
            return 0;
        }

        dungeon *dungeon = get_dungeon_manager().find(info->dungeon_id);
        if (dungeon == nullptr) {
            return 0;
        }

        regen *regen = info->regen;
        if (regen->time == 0) {
            regen->event = nullptr;
        }

        regen_spawn(regen, false, dungeon);
        return passes_per_sec(regen->time);
    }

    auto regen_do(const char *filename, int32_t map_index_value, int base_x, int base_y, dungeon *dungeon_value, bool once) -> bool {
        if (metin2::config::game().no_regen)
            return true;

        if (map_index_value >= 114 && map_index_value <= 117)
            return true;

        return load_regen_file(filename, map_index_value, base_x, base_y, true, [&](regen &entry) {
            if (entry.type == REGEN_TYPE_MOB && !find_regen_mob(entry))
                return;

            if (dungeon_value)
                entry.is_aggressive = true;

            regen *r = once ? &entry : new regen(entry);

            if (!once && dungeon_value != nullptr) {
                auto *info = alloc_event_info<dungeon_regen_event_info>();

                info->regen = r;
                info->dungeon_id = dungeon_value->id();

                r->event = event_create(dungeon_regen_event, info, passes_per_sec(number(0, 16)) + passes_per_sec(r->time));

                dungeon_value->add_regen(r);
            }

            regen_spawn(r, once, dungeon_value);
        });
    }

    auto regen_load_in_file(const char *filename, int32_t map_index_value, int base_x, int base_y) -> bool {
        if (metin2::config::game().no_regen)
            return true;

        return load_regen_file(filename, map_index_value, base_x, base_y, true, [](regen &entry) {
            entry.is_aggressive = true;

            if (entry.type == REGEN_TYPE_MOB && !find_regen_mob(entry))
                return;

            regen_spawn(&entry, true);
        });
    }

    EVENTFUNC(regen_event) {
        auto *info = dynamic_cast<regen_event_info *>(event->info.get());

        if (info == nullptr) {
            metin2::log::error("Regen event has no event info");
            return 0;
        }

        regen *regen = info->regen;

        if (regen->time == 0)
            regen->event = nullptr;

        regen_spawn(regen, false);
        return passes_per_sec(regen->time);
    }

    auto regen_load(const char *filename, int32_t map_index_value, int base_x, int base_y) -> bool {
        if (metin2::config::game().no_regen)
            return true;

        return load_regen_file(filename, map_index_value, base_x, base_y, false, [&](regen &entry) {
            if (metin2::config::test_server()) {
                get_mob_manager().inc_regen_count(entry.type, entry.vnum, entry.max_count, entry.time);
            }

            regen *r = regen_list.emplace_back(std::make_unique<regen>(entry)).get();

            if (r->type == REGEN_TYPE_MOB) {
                if (const mob *p = find_regen_mob(*r); p && (p->table.type == CHAR_TYPE_NPC || p->table.type == CHAR_TYPE_WARP || p->table.type == CHAR_TYPE_GOTO)) {
                    get_sectree_manager().insert_npc_position(map_index_value,
                        p->table.type,
                        p->table.locale_name,
                        (r->sx + r->ex) / 2 - base_x,
                        (r->sy + r->ey) / 2 - base_y);
                }
            }

            if (r->time != 0) {
                regen_spawn(r, false);

                auto *info = alloc_event_info<regen_event_info>();

                info->regen = r;

                r->event = event_create(regen_event, info, passes_per_sec(number(0, 16)) + passes_per_sec(r->time));
            }
        });
    }

    void regen_free() {
        for (auto &r : regen_list)
            event_cancel(&r->event);

        regen_list.clear();
    }

    void regen_reset(int x, int y) {
        for (auto &r : regen_list) {
            if (!r->event)
                continue;

            if (x != 0 || y != 0) {
                if (x >= r->sx && x <= r->ex && y >= r->sy && y <= r->ey)
                    event_reset_time(r->event, 1);
            } else
                event_reset_time(r->event, 1);
        }
    }
}
