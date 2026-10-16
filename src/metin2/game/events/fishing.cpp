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
#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
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
        void initialize() {
            fish_info_data fish_info_bak[MAX_FISH];
            std::memcpy(fish_info_bak, fish_info, sizeof(fish_info));

            memset(fish_info, 0, sizeof(fish_info));

            const int FILE_NAME_LEN = 256;
            char szFishingFileName[FILE_NAME_LEN + 1];
            snprintf(szFishingFileName, sizeof(szFishingFileName),
                "%s/fishing.txt", locale_service_get_base_path().c_str());
            FILE *fp = fopen(szFishingFileName, "r");

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

                char *p = strrchr(buf, '\n');
                *p = '\0';

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

                char szCol[256], szCol2[256];
                int iColCount = 0;

                do {
                    strlcpy(szCol2, start, std::min<int>(sizeof(szCol2), (tab - start) + 1));
                    szCol2[tab - start] = '\0';

                    trim_and_lower(szCol2, szCol, sizeof(szCol));

                    if (!*szCol || *szCol == '\t')
                        iColCount++;
                    else {
                        switch (iColCount++) {
                        case 0:  strlcpy(fish_info[idx].name, szCol, sizeof(fish_info[idx].name)); break;
                        case 1:  str_to_number(fish_info[idx].vnum, szCol); break;
                        case 2:  str_to_number(fish_info[idx].dead_vnum, szCol); break;
                        case 3:  str_to_number(fish_info[idx].grill_vnum, szCol); break;
                        case 4:  str_to_number(fish_info[idx].prob[0], szCol); break;
                        case 5:  str_to_number(fish_info[idx].prob[1], szCol); break;
                        case 6:  str_to_number(fish_info[idx].prob[2], szCol); break;
                        case 7:  str_to_number(fish_info[idx].prob[3], szCol); break;
                        case 8:  str_to_number(fish_info[idx].difficulty, szCol); break;
                        case 9:  str_to_number(fish_info[idx].time_type, szCol); break;
                        case 10: str_to_number(fish_info[idx].length_range[0], szCol); break;
                        case 11: str_to_number(fish_info[idx].length_range[1], szCol); break;
                        case 12: str_to_number(fish_info[idx].length_range[2], szCol); break;
                        case 13:
                        case 14:
                        case 15:
                        case 16:
                        case 17:
                        case 18:
                        case 19:
                        case 20:
                        case 21:
                        case 22:
                            str_to_number(fish_info[idx].used_table[iColCount - 1 - 12], szCol);
                            break;
                        }
                    }

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
                if (quest::quest_manager::instance().event_flag("manwoo") != 0)
                    prob_idx = 3;
                else
                    prob_idx = 2;
            }

            int adjust = 0;
            if (quest::quest_manager::instance().event_flag("fish_miss_pct") != 0) {
                int fish_pct_value = std::clamp(quest::quest_manager::instance().event_flag("fish_miss_pct"), 0, 200);
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
            fishing_event_info *info = dynamic_cast<fishing_event_info *>(event->info);

            if (info == nullptr) {
                metin2::log::error("Fishing event has no event info");
                return 0;
            }

            character *ch = character_manager::instance().find_by_pid(info->pid);

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
            fishing_event_info *info = alloc_event_info<fishing_event_info>();
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

            if (number(1, 100) <= fishing_time[fish_info[fish_id].time_type][time_step]) {
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
                int32_t ms = (int32_t)((get_dword_time() - info->hang_time));
                uint32_t item_vnum = 0;
                int ret = compute(info->fish_id, ms, &item_vnum, get_fishing_level(ch));

                switch (ret) {
                case -2:
                case -3:
                case -1:
                    {
                        int map_idx = ch->map_index();
                        int prob_idx = get_prob_index_by_map_index(map_idx);

                        log_manager::instance().fish_log(
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

                            if (quest::quest_manager::instance().event_flag("fishevent") > 0 && (info->fish_id == 5 || info->fish_id == 6)) {

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

                        log_manager::instance().fish_log(
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

                        log_manager::instance().fish_log(
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

                log_manager::instance().fish_log(
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

            for (std::map<std::string, int>::iterator it = fished.begin(); it != fished.end(); ++it)
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
                int u_index = std::lower_bound(s_acc_prob, s_acc_prob + NUM_USE_RESULT_COUNT, r) - s_acc_prob;

                switch (fish_info[idx].used_table[u_index]) {
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
                    ch->auto_give_item(fish_info[idx].used_table[u_index]);
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

                log_manager::instance().refine_log(ch->player_id(), item->name(), item->id(), -1, 1, "ROD_HACK");

                return 2;
            }

            game::items::item *rod = item;

            int iAdv = rod->value(0) / 10;

            if (number(1, 100) <= rod->value(3)) {
                log_manager::instance().refine_log(ch->player_id(), rod->name(), rod->id(), iAdv, 1, "ROD");

                game::items::item *pkNewItem = item_manager::instance().create_item(rod->refined_vnum(), 1);

                if (pkNewItem) {
                    uint8_t bCell = rod->cell();
                    item_manager::instance().remove_item(rod, "REMOVE (REFINE FISH_ROD)");
                    pkNewItem->add_to_character(ch, item_pos(INVENTORY, bCell));
                    log_manager::instance().item_log(ch, pkNewItem, "REFINE FISH_ROD SUCCESS", pkNewItem->name());
                    return 1;
                }

                return 2;
            } else {
                log_manager::instance().refine_log(ch->player_id(), rod->name(), rod->id(), iAdv, 0, "ROD");

                game::items::item *pkNewItem = item_manager::instance().create_item(rod->value(4), 1);
                if (pkNewItem) {
                    uint8_t bCell = rod->cell();
                    item_manager::instance().remove_item(rod, "REMOVE (REFINE FISH_ROD)");
                    pkNewItem->add_to_character(ch, item_pos(INVENTORY, bCell));
                    log_manager::instance().item_log(ch, pkNewItem, "REFINE FISH_ROD FAIL", pkNewItem->name());
                    return 0;
                }

                return 2;
            }
        }
    }
}
