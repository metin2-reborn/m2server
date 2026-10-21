#include <metin2/config/config.h>
#include <metin2/game/events/fishing.h>

#include <metin2/game/admin/cmd.h>
#include <metin2/game/app/config.h>
#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/characters/affect.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/items/item_manager.h>
#include <metin2/game/items/unique_item.h>
#include <metin2/game/persistence/log.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/text/locale_service.h>

#include <metin2/common/utils.h>
#include <metin2/core/utils.h>
#include <metin2/log/log.h>
#include <metin2/proto/character_types.h>
#include <metin2/proto/chat_types.h>
#include <metin2/proto/inventory.h>
#include <metin2/proto/item_types.h>
#include <metin2/proto/tables.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <format>
#include <map>
#include <numbers>
#include <string>

namespace metin2::game::inline events {

    namespace fishing {
        enum {
            MAX_FISH = 37,
            NUM_USE_RESULT_COUNT = 10,
            FISH_BONE_VNUM = 27799,
            SHELLFISH_VNUM = 27987,
            EARTHWORM_VNUM = 27801,
            WATER_STONE_VNUM_END = 28043,
            FISH_NAME_MAX_LEN = 64,
            MAX_PROB = 4,
        };

        enum {
            USED_NONE,
            USED_SHELLFISH,
            USED_WATER_STONE,
            USED_TREASURE_MAP,
            USED_EARTHWARM,
        };

        enum {
            FISHING_TIME_NORMAL,
            FISHING_TIME_SLOW,
            FISHING_TIME_QUICK,
            FISHING_TIME_ALL,
            FISHING_TIME_EASY,

            FISHING_TIME_COUNT,

            MAX_FISHING_TIME_COUNT = 31,
        };

        int fishing_time[FISHING_TIME_COUNT][MAX_FISHING_TIME_COUNT] =
            {
                {0,   0,   0,   0,   0,   2,   4,   8,   12,  16,  20,  22,  25,  30,  50,  80,  50,  30,  25,  22,  20,  16,  12,  8,   4,   2,   0,   0,   0,   0,   0  },
                {0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   4,   8,   12,  16,  20,  22,  25,  30,  50,  80,  50,  30,  25,  22,  20 },
                {20,  22,  25,  30,  50,  80,  50,  30,  25,  22,  20,  16,  12,  8,   4,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0  },
                {100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100},
                {20,  20,  20,  20,  20,  22,  24,  28,  32,  36,  40,  42,  45,  50,  70,  100, 70,  50,  45,  42,  40,  36,  32,  28,  24,  22,  20,  20,  20,  20,  20 },
        };

        struct fish_info_data {
            char name[FISH_NAME_MAX_LEN];

            uint32_t vnum;
            uint32_t dead_vnum;
            uint32_t grill_vnum;

            int prob[MAX_PROB];
            int difficulty;

            int time_type;
            int length_range[3];

            int used_table[NUM_USE_RESULT_COUNT];
        };

        int prob_sum[MAX_PROB];
        int prob_accumulate[MAX_PROB][MAX_FISH];

        fish_info_data fish_info[MAX_FISH] = {};
        using column_setter = void (*)(fish_info_data &, const char *);

        constexpr std::array<column_setter, 13> fish_columns = {
            [](fish_info_data &f, const char *v) { strlcpy(f.name, v, sizeof(f.name)); },
            [](fish_info_data &f, const char *v) { str_to_number(f.vnum, v); },
            [](fish_info_data &f, const char *v) { str_to_number(f.dead_vnum, v); },
            [](fish_info_data &f, const char *v) { str_to_number(f.grill_vnum, v); },
            [](fish_info_data &f, const char *v) { str_to_number(f.prob[0], v); },
            [](fish_info_data &f, const char *v) { str_to_number(f.prob[1], v); },
            [](fish_info_data &f, const char *v) { str_to_number(f.prob[2], v); },
            [](fish_info_data &f, const char *v) { str_to_number(f.prob[3], v); },
            [](fish_info_data &f, const char *v) { str_to_number(f.difficulty, v); },
            [](fish_info_data &f, const char *v) { str_to_number(f.time_type, v); },
            [](fish_info_data &f, const char *v) { str_to_number(f.length_range[0], v); },
            [](fish_info_data &f, const char *v) { str_to_number(f.length_range[1], v); },
            [](fish_info_data &f, const char *v) { str_to_number(f.length_range[2], v); },
        };

        void set_column(fish_info_data &fish, std::size_t column, const char *value) {
            if (column < fish_columns.size())
                fish_columns[column](fish, value);
            else if (column - fish_columns.size() < NUM_USE_RESULT_COUNT)
                str_to_number(fish.used_table[column - fish_columns.size()], value);
        }

        void initialize() {
            fish_info_data fish_info_bak[MAX_FISH];
            std::memcpy(fish_info_bak, fish_info, sizeof(fish_info));

            memset(fish_info, 0, sizeof(fish_info));

            const std::string fishing_file_name = std::format("{}/fishing.txt", locale_service_get_base_path());
            FILE *fp = fopen(fishing_file_name.c_str(), "r");

            if (*fish_info_bak[0].name)
                send_log("Reloading fish table.");

            if (!fp) {
                send_log("error! cannot open fishing.txt");

                if (*fish_info_bak[0].name) {
                    std::memcpy(fish_info, fish_info_bak, sizeof(fish_info));
                    send_log("  restoring to backup");
                }
                return;
            }

            memset(fish_info, 0, sizeof(fish_info));

            char buf[512];
            int idx = 0;

            while (fgets(buf, 512, fp)) {
                if (*buf == '#')
                    continue;

                buf[strcspn(buf, "\r\n")] = '\0';

                const char *start = buf;
                const char *tab = strchr(start, '\t');

                if (!tab) {
                    printf("Tab error on line: %s\n", buf);
                    send_log("error! parsing fishing.txt");

                    if (*fish_info_bak[0].name) {
                        std::memcpy(fish_info, fish_info_bak, sizeof(fish_info));
                        send_log("  restoring to backup");
                    }
                    break;
                }

                char col[256], col2[256];
                int col_count = 0;

                do {
                    strlcpy(col2, start, std::min<int>(sizeof(col2), (tab - start) + 1));
                    col2[tab - start] = '\0';

                    trim_and_lower(col2, col);

                    if (*col && *col != '\t')
                        set_column(fish_info[idx], col_count, col);
                    ++col_count;

                    start = tab + 1;
                    tab = strchr(start, '\t');
                } while (tab);

                idx++;

                if (idx == MAX_FISH)
                    break;
            }

            fclose(fp);

            for (int i = 0; i < MAX_FISH; ++i) {
                metin2::log::debug("Fishing: fish {}, vnum {:d}, prob {:d} {:d} {:d} {:d}, length {:d} {:d} {:d}",
                    fish_info[i].name,
                    fish_info[i].vnum,
                    fish_info[i].prob[0],
                    fish_info[i].prob[1],
                    fish_info[i].prob[2],
                    fish_info[i].prob[3],
                    fish_info[i].length_range[0],
                    fish_info[i].length_range[1],
                    fish_info[i].length_range[2]);
            }

            for (int j = 0; j < MAX_PROB; ++j) {
                prob_accumulate[j][0] = fish_info[0].prob[j];

                for (int i = 1; i < MAX_FISH; ++i)
                    prob_accumulate[j][i] = fish_info[i].prob[j] + prob_accumulate[j][i - 1];

                prob_sum[j] = prob_accumulate[j][MAX_FISH - 1];
                metin2::log::debug("Fishing: probability table {:d} sums to {:d}", j, prob_sum[j]);
            }
        }

        auto determine_fish_by_prob_index(int prob_idx) -> int {
            int rv = number(1, prob_sum[prob_idx]);
            int *p = std::lower_bound(prob_accumulate[prob_idx], prob_accumulate[prob_idx] + MAX_FISH, rv);
            int fish_idx = p - prob_accumulate[prob_idx];
            return fish_idx;
        }

        auto get_prob_index_by_map_index(int index) -> int {
            if (index > 60)
                return -1;

            switch (index) {
            case 1:
            case 21:
            case 41:
                return 0;

            case 3:
            case 23:
            case 43:
                return 1;
            }

            return -1;
        }

        auto determine_fish(character *ch) -> int {
            int map_idx = ch->map_index();
            int prob_idx = get_prob_index_by_map_index(map_idx);

            if (prob_idx < 0)
                return 0;

            if (ch->premium_remain_seconds(PREMIUM_FISH_MIND) > 0 ||
                ch->is_equip_unique_group(UNIQUE_GROUP_FISH_MIND)) {
                if (quest::get_quest_manager().event_flag("manwoo") != 0)
                    prob_idx = 3;
                else
                    prob_idx = 2;
            }

            int adjust = 0;
            if (quest::get_quest_manager().event_flag("fish_miss_pct") != 0) {
                int fish_pct_value = std::clamp(quest::get_quest_manager().event_flag("fish_miss_pct"), 0, 200);
                adjust = (100 - fish_pct_value) * fish_info[0].prob[prob_idx] / 100;
            }

            int rv = number(adjust + 1, prob_sum[prob_idx]);

            int *p = std::lower_bound(prob_accumulate[prob_idx], prob_accumulate[prob_idx] + MAX_FISH, rv);
            int fish_idx = p - prob_accumulate[prob_idx];

            uint32_t vnum = fish_info[fish_idx].vnum;

            if (vnum == 50008 || vnum == 50009 || vnum == 80008)
                return 0;

            return (fish_idx);
        }

        void fishing_react(character *ch) {
            packet_gc_fishing p;
            p.header = HEADER_GC_FISHING;
            p.subheader = FISHING_SUBHEADER_GC_REACT;
            p.info = ch->get_vid();
            ch->packet_around(&p, sizeof(p));
        }

        void fishing_success(character *ch) {
            packet_gc_fishing p;
            p.header = HEADER_GC_FISHING;
            p.subheader = FISHING_SUBHEADER_GC_SUCCESS;
            p.info = ch->get_vid();
            ch->packet_around(&p, sizeof(p));
        }

        void fishing_fail(character *ch) {
            packet_gc_fishing p;
            p.header = HEADER_GC_FISHING;
            p.subheader = FISHING_SUBHEADER_GC_FAIL;
            p.info = ch->get_vid();
            ch->packet_around(&p, sizeof(p));
        }

        void fishing_practice(character *ch) {
            item *rod = ch->wear(WEAR_WEAPON);
            if (rod && rod->type() == ITEM_ROD) {
                if (rod->refined_vnum() > 0 && rod->get_socket(0) < rod->value(2) && number(1, rod->value(1)) == 1) {
                    rod->set_socket(0, rod->get_socket(0) + 1);
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("낚시대의 수련도가 증가하였습니다! (%d/%d)"), rod->get_socket(0), rod->value(2));
                    if (rod->get_socket(0) == rod->value(2)) {
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("낚시대가 최대 수련도에 도달하였습니다."));
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("어부를 통해 다음 레벨의 낚시대로 업그레이드 할 수 있습니다."));
                    }
                }
            }
            rod->set_socket(2, 0);
        }

        auto predict_fish(character *ch) -> bool {
            if (ch->find_affect(AFFECT_FISH_MIND_PILL) ||
                ch->premium_remain_seconds(PREMIUM_FISH_MIND) > 0 ||
                ch->is_equip_unique_group(UNIQUE_GROUP_FISH_MIND))
                return true;

            return false;
        }

        EVENTFUNC(fishing_event) {
            auto *info = dynamic_cast<fishing_event_info *>(event->info.get());

            if (info == nullptr) {
                metin2::log::error("Fishing event has no event info");
                return 0;
            }

            character *ch = get_character_manager().find_by_pid(info->pid);

            if (!ch)
                return 0;

            item *rod = ch->wear(WEAR_WEAPON);

            if (!(rod && rod->type() == ITEM_ROD)) {
                ch->fishing_event = nullptr;
                return 0;
            }

            switch (info->step) {
            case 0:
                ++info->step;

                info->hang_time = get_dword_time();
                info->fish_id = determine_fish(ch);
                fishing_react(ch);

                if (predict_fish(ch)) {
                    packet_gc_fishing p;
                    p.header = HEADER_GC_FISHING;
                    p.subheader = FISHING_SUBHEADER_GC_FISH;
                    p.info = fish_info[info->fish_id].vnum;
                    ch->get_desc()->packet(&p, sizeof(packet_gc_fishing));
                }
                return (passes_per_sec(6));

            default:
                ++info->step;

                if (info->step > 5)
                    info->step = 5;

                ch->fishing_event = nullptr;
                fishing_fail(ch);
                rod->set_socket(2, 0);
                return 0;
            }
        }

        auto create_fishing_event(character *ch) -> event_ptr {
            auto *info = alloc_event_info<fishing_event_info>();
            info->pid = ch->player_id();
            info->step = 0;
            info->hang_time = 0;

            int time = number(10, 40);

            packet_gc_fishing p;
            p.header = HEADER_GC_FISHING;
            p.subheader = FISHING_SUBHEADER_GC_START;
            p.info = ch->get_vid();
            p.dir = (uint8_t)(ch->rotation() / 5);
            ch->packet_around(&p, sizeof(packet_gc_fishing));

            return event_create(fishing_event, info, passes_per_sec(time));
        }

        auto get_fishing_level(character *ch) -> int {
            item *rod = ch->wear(WEAR_WEAPON);

            if (!rod || rod->type() != ITEM_ROD)
                return 0;

            return rod->get_socket(2) + rod->value(0);
        }

        auto compute(uint32_t fish_id, uint32_t ms, uint32_t *item, int level) -> int {
            if (fish_id == 0)
                return -2;

            if (fish_id >= MAX_FISH) {
                metin2::log::error("Invalid fish id {:d}", fish_id);
                return -2;
            }

            if (ms > 6000)
                return -1;

            int time_step = std::clamp<int>(((ms + 99) / 200), 0, MAX_FISHING_TIME_COUNT - 1);

            if (prob(fishing_time[fish_info[fish_id].time_type][time_step])) {
                if (number(1, fish_info[fish_id].difficulty) <= level) {
                    *item = fish_info[fish_id].vnum;
                    return 0;
                }
                return -3;
            }

            return -1;
        }

        auto get_fish_length(int fish_id) -> int {
            if (number(0, 99)) {
                return (int)(fish_info[fish_id].length_range[0] +
                             (fish_info[fish_id].length_range[1] - fish_info[fish_id].length_range[0]) * (number(0, 2000) + number(0, 2000) + number(0, 2000) + number(0, 2000) + number(0, 2000)) / 10000);
            } else {
                return (int)(fish_info[fish_id].length_range[1] +
                             (fish_info[fish_id].length_range[2] - fish_info[fish_id].length_range[1]) * 2 * asin(number(0, 10000) / 10000.) / std::numbers::pi);
            }
        }

        void take(fishing_event_info *info, character *ch) {
            if (info->step == 1) {
                auto ms = (int32_t)((get_dword_time() - info->hang_time));
                uint32_t item_vnum = 0;
                int ret = compute(info->fish_id, ms, &item_vnum, get_fishing_level(ch));

                switch (ret) {
                case -2:
                case -3:
                case -1:
                    {
                        int map_idx = ch->map_index();
                        int prob_idx = get_prob_index_by_map_index(map_idx);

                        get_log_manager().fish_log(
                            ch->player_id(),
                            prob_idx,
                            info->fish_id,
                            get_fishing_level(ch),
                            ms);
                    }
                    fishing_fail(ch);
                    break;

                case 0:
                    if (item_vnum) {
                        fishing_success(ch);

                        packet_gc_fishing p;
                        p.header = HEADER_GC_FISHING;
                        p.subheader = FISHING_SUBHEADER_GC_FISH;
                        p.info = item_vnum;
                        ch->get_desc()->packet(&p, sizeof(packet_gc_fishing));

                        item *item = ch->auto_give_item(item_vnum, 1, -1, false);
                        if (item) {
                            item->set_socket(0, get_fish_length(info->fish_id));
                            if (metin2::config::test_server()) {
                                ch->chat_packet(CHAT_TYPE_INFO, lc_text("이번에 잡은 물고기의 길이는 %.2fcm"), item->get_socket(0) / 100.f);
                            }

                            if (quest::get_quest_manager().event_flag("fishevent") > 0 && (info->fish_id == 5 || info->fish_id == 6)) {

                                packet_gd_highscore p;
                                p.pid = ch->player_id();
                                p.value = item->get_socket(0);

                                if (info->fish_id == 5) {
                                    strlcpy(p.board, lc_text("낚시이벤트월척붕어"), sizeof(p.board));
                                } else if (info->fish_id == 6) {
                                    strlcpy(p.board, lc_text("낚시이벤트잉어"), sizeof(p.board));
                                }

                                db_clientdesc->db_packet(HEADER_GD_HIGHSCORE_REGISTER, 0, &p, sizeof(packet_gd_highscore));
                            }
                        }

                        int map_idx = ch->map_index();
                        int prob_idx = get_prob_index_by_map_index(map_idx);

                        get_log_manager().fish_log(
                            ch->player_id(),
                            prob_idx,
                            info->fish_id,
                            get_fishing_level(ch),
                            ms,
                            true,
                            item ? item->get_socket(0) : 0);

                    } else {
                        int map_idx = ch->map_index();
                        int prob_idx = get_prob_index_by_map_index(map_idx);

                        get_log_manager().fish_log(
                            ch->player_id(),
                            prob_idx,
                            info->fish_id,
                            get_fishing_level(ch),
                            ms);
                        fishing_fail(ch);
                    }
                    break;
                }
            } else if (info->step > 1) {
                int map_idx = ch->map_index();
                int prob_idx = get_prob_index_by_map_index(map_idx);

                get_log_manager().fish_log(
                    ch->player_id(),
                    prob_idx,
                    info->fish_id,
                    get_fishing_level(ch),
                    7000);
                fishing_fail(ch);
            } else {
                packet_gc_fishing p;
                p.header = HEADER_GC_FISHING;
                p.subheader = FISHING_SUBHEADER_GC_STOP;
                p.info = ch->get_vid();
                ch->packet_around(&p, sizeof(p));
            }

            if (info->step) {
                fishing_practice(ch);
            }
        }

        void simulation(int level, int count, int prob_idx, character *ch) {
            std::map<std::string, int> fished;
            int total_count = 0;

            for (int i = 0; i < count; ++i) {
                int fish_id = determine_fish_by_prob_index(prob_idx);
                uint32_t item = 0;
                compute(fish_id, (number(2000, 4000) + number(2000, 4000)) / 2, &item, level);

                if (item) {
                    fished[fish_info[fish_id].name]++;
                    total_count++;
                }
            }

            for (auto it = fished.begin(); it != fished.end(); ++it)
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("%s : %d 마리"), it->first.c_str(), it->second);

            ch->chat_packet(CHAT_TYPE_INFO, lc_text("%d 종류 %d 마리 낚음"), fished.size(), total_count);
        }

        void use_fish(character *ch, item *item) {
            int idx = item->vnum() - fish_info[2].vnum + 2;

            if (idx <= 1 || idx >= MAX_FISH)
                return;

            int r = number(1, 10000);

            item->set_count(item->count() - 1);

            if (r >= 4001) {
                ch->auto_give_item(fish_info[idx].dead_vnum);
            } else if (r >= 2001) {
                ch->auto_give_item(FISH_BONE_VNUM);
            } else {
                static int s_acc_prob[NUM_USE_RESULT_COUNT] = {1000, 1500, 1800, 1900, 1950, 1980, 1990, 1995, 1999, 2000};
                int index = std::lower_bound(s_acc_prob, s_acc_prob + NUM_USE_RESULT_COUNT, r) - s_acc_prob;

                switch (fish_info[idx].used_table[index]) {
                case USED_TREASURE_MAP:
                case USED_NONE:
                case USED_WATER_STONE:
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("고기가 흔적도 없이 사라집니다."));
                    break;

                case USED_SHELLFISH:

                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("배 속에서 조개가 나왔습니다."));
                    ch->auto_give_item(SHELLFISH_VNUM);
                    break;

                case USED_EARTHWARM:
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("배 속에서 지렁이가 나왔습니다."));
                    ch->auto_give_item(EARTHWORM_VNUM);
                    break;

                default:
                    ch->auto_give_item(fish_info[idx].used_table[index]);
                    break;
                }
            }
        }

        void grill(character *ch, item *item) {
            int idx = -1;
            uint32_t vnum = item->vnum();
            if (vnum >= 27803 && vnum <= 27830)
                idx = vnum - 27800;
            if (vnum >= 27833 && vnum <= 27860)
                idx = vnum - 27830;
            if (idx == -1)
                return;

            int count = item->count();

            ch->chat_packet(CHAT_TYPE_INFO, lc_text("%s를 구웠습니다."), item->name());
            item->set_count(0);
            ch->auto_give_item(fish_info[idx].grill_vnum, count);
        }

        auto refinable_rod(item *rod) -> bool {
            if (rod->type() != ITEM_ROD)
                return false;

            if (rod->is_equipped())
                return false;

            return (rod->get_socket(0) == rod->value(2));
        }

        auto real_refine_rod(character *ch, item *item) -> int {
            if (!ch || !item)
                return 2;

            if (!refinable_rod(item)) {
                metin2::log::warn("Possible hack: pid {:d} tried to refine non-refinable rod {} (id {:d})", ch->player_id(), item->name(), item->id());

                get_log_manager().refine_log(ch->player_id(), item->name(), item->id(), -1, 1, "ROD_HACK");

                return 2;
            }

            game::items::item *rod = item;

            int adv = rod->value(0) / 10;

            if (prob(rod->value(3))) {
                get_log_manager().refine_log(ch->player_id(), rod->name(), rod->id(), adv, 1, "ROD");

                game::items::item *new_item = get_item_manager().create_item(rod->refined_vnum(), 1);

                if (new_item) {
                    uint8_t cell = rod->cell();
                    get_item_manager().remove_item(rod, "REMOVE (REFINE FISH_ROD)");
                    new_item->add_to_character(ch, item_pos(INVENTORY, cell));
                    get_log_manager().item_log(ch, new_item, "REFINE FISH_ROD SUCCESS", new_item->name());
                    return 1;
                }

                return 2;
            } else {
                get_log_manager().refine_log(ch->player_id(), rod->name(), rod->id(), adv, 0, "ROD");

                game::items::item *new_item = get_item_manager().create_item(rod->value(4), 1);
                if (new_item) {
                    uint8_t cell = rod->cell();
                    get_item_manager().remove_item(rod, "REMOVE (REFINE FISH_ROD)");
                    new_item->add_to_character(ch, item_pos(INVENTORY, cell));
                    get_log_manager().item_log(ch, new_item, "REFINE FISH_ROD FAIL", new_item->name());
                    return 0;
                }

                return 2;
            }
        }
    }
}
