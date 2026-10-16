#include <metin2/config/config.h>
#include <metin2/game/events/xmas_event.h>

#include <metin2/game/app/config.h>
#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/desc_manager.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/world/sectree_manager.h>

#include <metin2/log/log.h>
#include <metin2/proto/chat_types.h>

#include <cstdint>
#include <string>

namespace metin2::game::inline events {

    namespace xmas {
        void process_event_flag(const std::string &name, int prev_value, int value) {
            if (name == "xmas_snow" || name == "xmas_boom" || name == "xmas_song" || name == "xmas_tree") {
                const desc_manager::desc_set &descs = get_desc_manager().client_set();

                for (auto it = descs.begin(); it != descs.end(); ++it) {
                    character *ch = (*it)->get_character();

                    if (!ch)
                        continue;

                    ch->chat_packet(CHAT_TYPE_COMMAND, "%s %d", name.c_str(), value);
                }

                if (name == "xmas_boom") {
                    if (value && !prev_value) {
                        spawn_event_helper(true);
                    } else if (!value && prev_value) {
                        spawn_event_helper(false);
                    }
                } else if (name == "xmas_tree") {
                    if (value > 0 && prev_value == 0) {
                        character_vector_interactor i;

                        if (!get_character_manager().characters_by_race_num(MOB_XMAS_TREE_VNUM, i))
                            get_character_manager().spawn_mob(MOB_XMAS_TREE_VNUM, 61, 76500 + 358400, 60900 + 153600, 0, false, -1);
                    } else if (prev_value > 0 && value == 0) {
                        character_vector_interactor i;

                        if (get_character_manager().characters_by_race_num(MOB_XMAS_TREE_VNUM, i)) {
                            auto it = i.begin();

                            while (it != i.end())
                                get_character_manager().destroy_character(*it++);
                        }
                    }
                }
            } else if (name == "xmas_santa") {
                switch (value) {
                case 0:
                    {
                        character_vector_interactor i;

                        if (get_character_manager().characters_by_race_num(MOB_SANTA_VNUM, i)) {
                            auto it = i.begin();

                            while (it != i.end())
                                get_character_manager().destroy_character(*it++);
                        }
                    }

                    break;

                case 1:
                    if (map_allow_find(61)) {
                        quest::get_quest_manager().request_set_event_flag("xmas_santa", 2);

                        character_vector_interactor i;

                        if (get_character_manager().characters_by_race_num(MOB_SANTA_VNUM, i))
                            get_character_manager().spawn_mob_random_position(MOB_SANTA_VNUM, 61);
                    }
                    break;

                case 2:
                    break;
                }
            }
        }

        EVENTINFO(spawn_santa_info) {
            int32_t map_index;

            spawn_santa_info()
                : map_index(0) {
            }
        };

        EVENTFUNC(spawn_santa_event) {
            auto *info = dynamic_cast<spawn_santa_info *>(event->info.get());

            if (info == nullptr) {
                metin2::log::error("Santa spawn event has no event info");
                return 0;
            }

            int32_t map_index = info->map_index;

            if (quest::get_quest_manager().event_flag("xmas_santa") == 0)
                return 0;

            character_vector_interactor i;

            if (get_character_manager().characters_by_race_num(MOB_SANTA_VNUM, i))
                return 0;

            if (get_character_manager().spawn_mob_random_position(xmas::MOB_SANTA_VNUM, map_index)) {
                metin2::log::info("Xmas: santa spawned");
                return 0;
            }

            return passes_per_sec(5);
        }

        void spawn_santa(int32_t map_index_value, int time_gap_sec) {
            if (metin2::config::test_server()) {
                time_gap_sec /= 60;
            }

            metin2::log::info("Xmas: santa respawns in {:d} seconds", time_gap_sec);
            auto *info = alloc_event_info<spawn_santa_info>();

            info->map_index = map_index_value;

            event_create(spawn_santa_event, info, passes_per_sec(time_gap_sec));
        }

        void spawn_event_helper(bool spawn) {
            if (spawn == true) {
                struct npc_sell_firework_position {
                    int32_t map_index;
                    int x;
                    int y;
                } positions[] = {
                    {1,  615, 618},
                    {3,  500, 625},
                    {21, 598, 665},
                    {23, 476, 360},
                    {41, 318, 629},
                    {43, 478, 375},
                    {0,  0,   0  },
                };

                npc_sell_firework_position *p = positions;
                while (p->map_index) {
                    if (map_allow_find(p->map_index)) {
                        pixel_position pos_base;
                        if (!get_sectree_manager().map_base_position_by_map_index(p->map_index, pos_base)) {
                            metin2::log::error("Cannot get base position of map {:d}", p->map_index);
                            p++;
                            continue;
                        }

                        get_character_manager().spawn_mob(
                            MOB_XMAS_FIRWORK_SELLER_VNUM, p->map_index, pos_base.x + p->x * 100, pos_base.y + p->y * 100, 0, false, -1);
                    }
                    p++;
                }
            } else {
                character_vector_interactor i;

                if (get_character_manager().characters_by_race_num(MOB_XMAS_FIRWORK_SELLER_VNUM, i)) {
                    auto it = i.begin();

                    while (it != i.end())
                        get_character_manager().destroy_character(*it++);
                }
            }
        }
    }
}
