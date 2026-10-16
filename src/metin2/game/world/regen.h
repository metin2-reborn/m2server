#pragma once

#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/world/dungeon.h>

#include <cstddef>
#include <cstdint>

namespace metin2::game::inline world {

    enum {
        REGEN_TYPE_MOB,
        REGEN_TYPE_GROUP,
        REGEN_TYPE_EXCEPTION,
        REGEN_TYPE_GROUP_GROUP,
        REGEN_TYPE_ANYWHERE,
        REGEN_TYPE_MAX_NUM
    };

    struct regen {
        int32_t map_index = 0;
        int type = 0;
        int sx = 0, sy = 0, ex = 0, ey = 0;
        uint8_t z_section = 0;
        uint8_t direction = 0;
        uint32_t time = 0;
        int max_count = 0;
        int count = 0;
        int vnum = 0;
        bool is_aggressive = false;
        event_ptr event = nullptr;
        size_t id = 0;
    };

    EVENTINFO(regen_event_info) {
        game::world::regen *regen;

        regen_event_info()
            : regen(nullptr) {
        }
    };

    class dungeon;

    EVENTINFO(dungeon_regen_event_info) {
        game::world::regen *regen;
        dungeon::id_type dungeon_id;

        dungeon_regen_event_info()
            : regen(nullptr), dungeon_id(0) {
        }
    };

    extern auto regen_load(const char *filename, int32_t map_index_value, int base_x, int base_y) -> bool;
    extern auto regen_do(const char *filename, int32_t map_index_value, int base_x, int base_y, dungeon *dungeon_value, bool once = true) -> bool;
    extern auto regen_load_in_file(const char *filename, int32_t map_index_value, int base_x, int base_y) -> bool;
    extern void regen_free();

    extern void regen_reset(int x, int y);
}
