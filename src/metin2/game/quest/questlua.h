#pragma once

#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/protocol/buffer_manager.h>
#include <metin2/game/quest/lua_incl.h>

#include <cstdint>
#include <sstream>
#include <string>

namespace metin2::game {

    namespace quest {
        auto quest_setstate(lua_State *lua) -> int;
        extern void register_pc_function_table();
        extern void register_npc_function_table();
        extern void register_target_function_table();
        extern void register_affect_function_table();
        extern void register_building_function_table();
        extern void register_marriage_function_table();
        extern void register_item_function_table();
        extern void register_dungeon_function_table();
        extern void register_quest_function_table();
        extern void register_party_function_table();
        extern void register_horse_function_table();
        extern void register_pet_function_table();
        extern void register_guild_function_table();
        extern void register_game_function_table();
        extern void register_arena_function_table();
        extern void register_global_function_table(lua_State *lua);
        extern void register_forked_function_table();
        extern void register_monarch_function_table();
        extern void register_ox_event_function_table();
        extern void register_mgmt_function_table();
        extern void register_battle_arena_function_table();
        extern void register_dance_event_function_table();
        extern void register_dragon_lair_function_table();
        extern void register_speed_server_function_table();
        extern void register_dragon_soul_function_table();

        extern void combine_lua_string(lua_State *lua, std::ostringstream &s);

        void set_pc_quest_flag(character *ch, const std::string &flagname, int value);
        void purge_monsters_in_area(sectree_map *map, int x1, int y1, int x2, int y2, character *except_char);

        void send_buffer_to_map(sectree_map *map, const temp_buffer &buf, uint8_t empire = 0);

        EVENTINFO(warp_all_to_map_my_empire_event_info) {
            uint8_t empire;
            int32_t map_index_from;
            int32_t map_index_to;
            int32_t x;
            int32_t y;

            warp_all_to_map_my_empire_event_info()
                : empire(0), map_index_from(0), map_index_to(0), x(0), y(0) {
            }
        };

        EVENTFUNC(warp_all_to_map_my_empire_event);
    }
}
