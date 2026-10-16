#include <metin2/game/events/mining.h>

#include <metin2/game/app/config.h>
#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/combat/skill.h>
#include <metin2/game/items/item.h>
#include <metin2/game/items/item_manager.h>
#include <metin2/game/persistence/db.h>
#include <metin2/game/persistence/log.h>
#include <metin2/game/text/locale.hpp>

#include <metin2/core/utils.h>
#include <metin2/log/log.h>
#include <metin2/proto/character_types.h>
#include <metin2/proto/chat_types.h>
#include <metin2/proto/inventory.h>
#include <metin2/proto/item_types.h>
#include <metin2/proto/log_types.h>

#include <algorithm>
#include <array>
#include <cstdint>

namespace metin2::game::inline events {

    namespace mining {
        enum {
            ORE_COUNT_FOR_REFINE = 100,
        };

        struct mining_info {
            uint32_t load_vnum;
            uint32_t raw_ore_vnum;
            uint32_t refine_vnum;
        };

        constexpr std::array<mining_info, 18> info = {
            {
             {20047, 50601, 50621},
             {20048, 50602, 50622},
             {20049, 50603, 50623},
             {20050, 50604, 50624},
             {20051, 50605, 50625},
             {20052, 50606, 50626},
             {20053, 50607, 50627},
             {20054, 50608, 50628},
             {20055, 50609, 50629},
             {20056, 50610, 50630},
             {20057, 50611, 50631},
             {20058, 50612, 50632},
             {20059, 50613, 50633},
             {30301, 50614, 50634},
             {30302, 50615, 50635},
             {30303, 50616, 50636},
             {30304, 50617, 50637},
             {30305, 50618, 50638},
             }
        };

        struct fraction {
            int pct;
            int min_count;
            int max_count;
        };

        constexpr std::array<fraction, 9> fraction_info = {
            {
             {20, 1, 10},
             {30, 11, 20},
             {20, 21, 30},
             {15, 31, 40},
             {5, 41, 50},
             {4, 51, 60},
             {3, 61, 70},
             {2, 71, 80},
             {1, 81, 90},
             }
        };

        constexpr std::array<int, 10> pick_grade_add_pct = {3, 5, 8, 11, 15, 20, 26, 32, 40, 50};

        constexpr std::array<int, SKILL_MAX_LEVEL + 1> skill_level_add_pct = {
            0,
            1,
            1,
            1,
            1,
            2,
            2,
            2,
            2,
            3,
            3,
            3,
            3,
            4,
            4,
            4,
            4,
            5,
            5,
            5,
            5,
            6,
            6,
            6,
            6,
            7,
            7,
            7,
            7,
            8,
            8,
            8,
            8,
            9,
            9,
            9,
            9,
            10,
            10,
            10,
            11,
        };

        auto get_raw_ore_from_load(uint32_t load_vnum_value) -> uint32_t {
            const auto it = std::ranges::find(info, load_vnum_value, &mining_info::load_vnum);
            return it != info.end() ? it->raw_ore_vnum : 0;
        }

        auto get_refine_from_raw_ore(uint32_t raw_ore_vnum_value) -> uint32_t {
            const auto it = std::ranges::find(info, raw_ore_vnum_value, &mining_info::raw_ore_vnum);
            return it != info.end() ? it->refine_vnum : 0;
        }

        auto get_fraction_count() -> int {
            int r = number(1, 100);

            for (const auto &f : fraction_info) {
                if (r <= f.pct)
                    return number(f.min_count, f.max_count);
                r -= f.pct;
            }

            return 0;
        }

        void ore_drop(character *ch, uint32_t load_vnum) {
            uint32_t raw_ore_vnum = get_raw_ore_from_load(load_vnum);

            int fraction_count = get_fraction_count();

            if (fraction_count == 0) {
                metin2::log::error("Ore has a fraction count of zero");
                return;
            }

            item *item = item_manager::instance().create_item(raw_ore_vnum, fraction_count);

            if (!item) {
                metin2::log::error("Cannot create raw ore item (vnum {:d})", raw_ore_vnum);
                return;
            }

            pixel_position pos;
            pos.x = ch->x() + number(-200, 200);
            pos.y = ch->y() + number(-200, 200);

            item->add_to_ground(ch->map_index(), pos);
            item->start_destroy_event();
            item->set_ownership(ch, 15);

            db_manager::instance().send_money_log(MONEY_LOG_DROP, item->vnum(), item->count());
        }

        auto get_ore_pct(character *ch) -> int {
            int default_pct = 20;
            int skill_level = ch->skill_level(SKILL_MINING);

            item *pick = ch->wear(WEAR_WEAPON);

            if (!pick || pick->type() != ITEM_PICK)
                return 0;

            return default_pct + skill_level_add_pct[std::clamp(skill_level, 0, 40)] + pick_grade_add_pct[std::clamp(pick->refine_level(), 0, 9)];
        }

        EVENTINFO(mining_event_info) {
            uint32_t pid;
            uint32_t vid_load;

            mining_event_info()
                : pid(0), vid_load(0) {
            }
        };

        auto pick_check(item &item) -> bool {
            if (item.type() != ITEM_PICK)
                return false;

            return true;
        }

        auto pick_get_max_exp(item &pick) -> int {
            return pick.value(2);
        }

        auto pick_get_cur_exp(item &pick) -> int {
            return pick.get_socket(0);
        }

        void pick_inc_cur_exp(item &pick) {
            int cur = pick_get_cur_exp(pick);
            pick.set_socket(0, cur + 1);
        }

        void pick_max_cur_exp(item &pick) {
            int max = pick_get_max_exp(pick);
            pick.set_socket(0, max);
        }

        auto pick_refinable(item &item) -> bool {
            if (pick_get_cur_exp(item) < pick_get_max_exp(item))
                return false;

            return true;
        }

        auto pick_is_practice_success(item &pick) -> bool {
            return (number(1, pick.value(1)) == 1);
        }

        auto pick_is_refine_success(item &pick) -> bool {
            return (prob(pick.value(3)));
        }

        auto real_refine_pick(character *ch, item *item) -> int {
            if (!ch || !item)
                return 2;

            log_manager &log_mgr = log_manager::instance();
            item_manager &item_mgr = item_manager::instance();

            if (!pick_check(*item)) {
                metin2::log::warn("Possible hack: pid {:d} tried to refine invalid pick {} (id {:d}, type {:d})", ch->player_id(), item->name(), item->id(), item->type());
                log_mgr.refine_log(ch->player_id(), item->name(), item->id(), -1, 1, "PICK_HACK");
                return 2;
            }

            game::items::item &old_pick = *item;

            if (!pick_refinable(old_pick))
                return 2;

            int adv = old_pick.value(0) / 10;

            if (old_pick.is_equipped() == true)
                return 2;

            if (pick_is_refine_success(old_pick)) {
                log_mgr.refine_log(ch->player_id(), old_pick.name(), old_pick.id(), adv, 1, "PICK");

                game::items::item *new_pick = item_manager::instance().create_item(old_pick.refined_vnum(), 1);
                if (new_pick) {
                    uint8_t cell = old_pick.cell();
                    item_mgr.remove_item(item, "REMOVE (REFINE PICK)");
                    new_pick->add_to_character(ch, item_pos(INVENTORY, cell));
                    log_manager::instance().item_log(ch, new_pick, "REFINE PICK SUCCESS", new_pick->name());
                    return 1;
                }

                return 2;
            } else {
                log_mgr.refine_log(ch->player_id(), old_pick.name(), old_pick.id(), adv, 0, "PICK");

                game::items::item *new_pick = item_manager::instance().create_item(old_pick.value(4), 1);

                if (new_pick) {
                    uint8_t cell = old_pick.cell();
                    item_mgr.remove_item(item, "REMOVE (REFINE PICK)");
                    new_pick->add_to_character(ch, item_pos(INVENTORY, cell));
                    log_mgr.item_log(ch, new_pick, "REFINE PICK FAIL", new_pick->name());
                    return 0;
                }

                return 2;
            }
        }

        void cheat_max_pick(character *ch, item *item) {
            if (!item)
                return;

            if (!pick_check(*item))
                return;

            game::items::item &pick = *item;
            pick_max_cur_exp(pick);

            ch->chat_packet(CHAT_TYPE_INFO, lc_text("곡괭이 수련도가 최대(%d)가 되었습니다."), pick_get_cur_exp(pick));
        }

        void practice_pick(character *ch, item *item) {
            if (!item)
                return;

            if (!pick_check(*item))
                return;

            game::items::item &pick = *item;
            if (pick.refined_vnum() <= 0)
                return;

            if (pick_is_practice_success(pick)) {

                if (pick_refinable(pick)) {
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("곡괭이가 최대 수련도에 도달하였습니다."));
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("나무꾼를 통해 다음 레벨의 곡괭이로 업그레이드 할 수 있습니다."));
                } else {
                    pick_inc_cur_exp(pick);

                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("곡괭이의 수련도가 증가하였습니다! (%d/%d)"),
                        pick_get_cur_exp(pick), pick_get_max_exp(pick));

                    if (pick_refinable(pick)) {
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("곡괭이가 최대 수련도에 도달하였습니다."));
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("나무꾼를 통해 다음 레벨의 곡괭이로 업그레이드 할 수 있습니다."));
                    }
                }
            }
        }

        EVENTFUNC(mining_event) {
            auto *info = dynamic_cast<mining_event_info *>(event->info.get());

            if (info == nullptr) {
                metin2::log::error("Mining event has no event info");
                return 0;
            }

            character *ch = character_manager::instance().find_by_pid(info->pid);
            character *load = character_manager::instance().find(info->vid_load);

            if (!ch)
                return 0;

            ch->mining_take();

            item *pick = ch->wear(WEAR_WEAPON);

            if (!pick || !pick_check(*pick)) {
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("곡괭이를 들고 있지 않아서 캘 수 없습니다."));
                return 0;
            }

            if (!load) {
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("더이상 캐낼 수 없습니다."));
                return 0;
            }

            int pct = get_ore_pct(ch);

            if (prob(pct)) {
                ore_drop(ch, load->race_num());
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("채광에 성공하였습니다."));
            } else {
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("채광에 실패하였습니다."));
            }

            practice_pick(ch, pick);

            return 0;
        }

        auto create_mining_event(character *ch, character *load, int count) -> event_ptr {
            auto *info = alloc_event_info<mining_event_info>();
            info->pid = ch->player_id();
            info->vid_load = load->get_vid();

            return event_create(mining_event, info, passes_per_sec(2 * count));
        }

        auto ore_refine(character *ch, character *npc, item *ore, int cost, int pct, game::items::item *metinstone_item) -> bool {
            if (!ch || !npc)
                return false;

            if (ore->owner() != ch) {
                metin2::log::warn("Player {} tried to refine ore they do not own", ch->name());
                return false;
            }

            if (ore->count() < ORE_COUNT_FOR_REFINE) {
                metin2::log::warn("Player {} tried to refine without enough ore", ch->name());
                return false;
            }

            uint32_t refined_vnum = get_refine_from_raw_ore(ore->vnum());

            if (refined_vnum == 0)
                return false;

            ch->set_refine_npc(npc);
            ore->set_count(ore->count() - ORE_COUNT_FOR_REFINE);
            int cost_value = ch->compute_refine_fee(cost, 1);

            if (ch->gold() < cost_value)
                return false;

            ch->pay_refine_fee(cost_value);

            if (metinstone_item)
                item_manager::instance().remove_item(metinstone_item, "REMOVE (MELT)");

            if (prob(pct)) {
                ch->auto_give_item(refined_vnum, 1);
                return true;
            }

            return false;
        }

        auto is_vein_of_ore(uint32_t vnum) -> bool {
            return std::ranges::contains(info, vnum, &mining_info::load_vnum);
        }
    }
}
