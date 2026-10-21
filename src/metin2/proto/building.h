#pragma once

#include <cstdint>

namespace metin2::inline proto {
    namespace building {
        inline constexpr int OBJECT_MATERIAL_MAX_NUM = 5;

        struct land_table {
            uint32_t id;
            int32_t map_index;
            int32_t x, y;
            int32_t width, height;
            uint32_t guild_id;
            uint8_t guild_level_limit;
            uint32_t price;
        };

        struct object_material {
            uint32_t item_vnum;
            uint32_t count;
        };

        struct object_proto {
            uint32_t vnum;
            uint32_t price;

            object_material materials[OBJECT_MATERIAL_MAX_NUM];

            uint32_t upgrade_vnum;
            uint32_t upgrade_limit_time;
            int32_t life;
            int32_t region[4];

            uint32_t npc_vnum;
            int32_t npcx;
            int32_t npcy;

            uint32_t group_vnum;
            uint32_t depend_on_group_vnum;
        };

        struct object_table {
            uint32_t id;
            uint32_t land_id;
            uint32_t vnum;
            int32_t map_index;
            int32_t x, y;

            float x_rot;
            float y_rot;
            float z_rot;
            int32_t life;
        };
    }
}
