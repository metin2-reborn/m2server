#include <metin2/config/config.h>
#include <metin2/game/events/battle_arena.h>

#include <metin2/game/admin/cmd.h>
#include <metin2/game/app/config.h>
#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/empire/start_position.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/text/locale_service.h>
#include <metin2/game/world/regen.h>
#include <metin2/game/world/sectree_manager.h>

#include <metin2/core/utils.h>
#include <metin2/proto/character_types.h>

#include <cstdint>
#include <cstdio>
#include <string>

namespace metin2::game::inline events {

    battle_arena::battle_arena()
        : event_(nullptr),
          status_(STATUS_CLOSE),
          map_index_(0),
          empire_(0),
          force_end_(false) {
    }

    auto battle_arena::is_running() -> bool {
        return status_ == STATUS_CLOSE ? false : true;
    }

    auto battle_arena::is_battle_arena_map(int map_index) -> bool {
        if (map_index == battle_arena_map[1] ||
            map_index == battle_arena_map[2] ||
            map_index == battle_arena_map[3]) {
            return true;
        }

        return false;
    }

    void battle_arena::set_status(battlearena_status status) {
        status_ = status;
    }

    EVENTINFO(battle_arena_info) {
        int empire;
        int map_index;
        int state;
        int wait_count;

        battle_arena_info()
            : empire(0), map_index(0), state(0), wait_count(0) {
        }
    };

    EVENTFUNC(battle_arena_event) {
        auto *info = dynamic_cast<battle_arena_info *>(event->info.get());

        if (info == nullptr) {
            return 0;
        }

        if (info != nullptr) {
            switch (info->state) {
            case 0:
                {
                    ++info->state;
                    broadcast_notice(lc_text("몬스터들의 공격까지 5분 남았습니다!!!"));
                }
                return metin2::config::test_server() ? passes_per_sec(60) : passes_per_sec(60 * 4);

            case 1:
                {
                    ++info->state;
                    broadcast_notice(lc_text("몬스터들의 공격까지 1분 남았습니다!!!"));
                }
                return metin2::config::test_server() ? passes_per_sec(10) : passes_per_sec(60);

            case 2:
                {
                    ++info->state;
                    info->wait_count = 0;

                    quest::quest_manager::instance().request_set_event_flag("battle_arena", 0);
                    broadcast_notice(lc_text("몬스터들이 성을 공격하기 시작했습니다."));

                    sectree_map *sectree = sectree_manager::instance().map(info->map_index);

                    if (sectree != nullptr) {
                        std::string map = locale_service_get_map_path();
                        if (info->empire > 0) {
                            map += str_regen[info->empire];

                            regen_do(map.c_str(), info->map_index, sectree->setting.base_x, sectree->setting.base_y, nullptr, false);
                        }
                    }
                }
                return metin2::config::test_server() ? passes_per_sec(60 * 1) : passes_per_sec(60 * 5);

            case 3:
                {
                    if (sectree_manager::instance().monster_count_in_map(info->map_index) <= 0) {
                        info->state = 6;
                        send_notice_map(lc_text("중앙 제단에 악의 기운이 모여듭니다."), info->map_index, false);
                    } else {
                        info->wait_count++;

                        if (info->wait_count >= 5) {
                            info->state++;
                            send_notice_map(lc_text("몬스터들이 물러갈 조짐을 보입니다."), info->map_index, false);
                        } else {
                            battle_arena::instance().spawn_random_stone();
                        }
                    }
                }
                return metin2::config::test_server() ? passes_per_sec(60) : passes_per_sec(60 * 5);

            case 4:
                {
                    info->state++;
                    send_notice_map(lc_text("몬스터들이 물러가기 시작했습니다."), info->map_index, false);
                    send_notice_map(lc_text("잠시 후 마을로 돌아갑니다."), info->map_index, false);

                    sectree_manager::instance().purge_monsters_in_map(info->map_index);
                }
                return passes_per_sec(30);

            case 5:
                {
                    sectree_map *sectree = sectree_manager::instance().map(info->map_index);

                    if (sectree != nullptr) {
                        sectree->for_each([](entity *ent) {
                            if (ent->is_type(ENTITY_CHARACTER) == true) {
                                auto *ch = (character *)ent;

                                if (ch->is_pc() == true) {
                                    if (!metin2::config::test_server()) {
                                        if (ch->gm_level() != GM_PLAYER) return;
                                    }

                                    int empire, map_index, x, y;

                                    empire = ch->empire();
                                    map_index = empire_start_map(empire);
                                    x = empire_start_x(empire);
                                    y = empire_start_y(empire);

                                    ch->warp_set(x, y, map_index);
                                }
                            }
                        });
                    }

                    battle_arena::instance().end();
                }
                return 0;

            case 6:
                {
                    info->state++;
                    info->wait_count = 0;

                    send_notice_map(lc_text("몬스터들의 대장이 나타났습니다."), info->map_index, false);
                    send_notice_map(lc_text("30분 내로 귀목령주를 물리쳐주세요."), info->map_index, false);

                    battle_arena::instance().spawn_last_boss();
                }
                return metin2::config::test_server() ? passes_per_sec(60) : passes_per_sec(60 * 5);

            case 7:
                {
                    if (sectree_manager::instance().monster_count_in_map(info->map_index) <= 0) {
                        send_notice_map(lc_text("귀목령주와 그의 부하들을 모두 물리쳤습니다."), info->map_index, false);
                        send_notice_map(lc_text("잠시 후 마을로 돌아갑니다."), info->map_index, false);

                        info->state = 5;

                        return passes_per_sec(60);
                    }

                    info->wait_count++;

                    if (info->wait_count >= 6) {
                        send_notice_map(lc_text("귀목령주가 퇴각하였습니다."), info->map_index, false);
                        send_notice_map(lc_text("잠시 후 마을로 돌아갑니다."), info->map_index, false);

                        sectree_manager::instance().purge_monsters_in_map(info->map_index);
                        sectree_manager::instance().purge_stones_in_map(info->map_index);

                        info->state = 5;

                        return passes_per_sec(60);
                    } else {
                        battle_arena::instance().spawn_random_stone();
                    }
                }
                return metin2::config::test_server() ? passes_per_sec(60) : passes_per_sec(60 * 5);
            }
        }

        return 0;
    }

    auto battle_arena::start(int empire_value) -> bool {
        if (status_ != STATUS_CLOSE) return false;
        if (empire_value < 1 || empire_value > 3) return false;

        map_index_ = battle_arena_map[empire_value];
        empire_ = empire_value;

        char buf[1024];
        snprintf(buf, sizeof(buf), lc_text("%s의 성으로 몬스터들이 진군하고 있습니다."), empire_name(empire_));
        broadcast_notice(buf);
        broadcast_notice(lc_text("10분 뒤 성을 공격할 예정입니다."));

        if (event_ != nullptr) {
            event_cancel(&event_);
        }

        auto *info = alloc_event_info<battle_arena_info>();

        info->map_index = map_index_;
        info->empire = empire_;
        info->state = 0;
        info->wait_count = 0;

        event_ = event_create(battle_arena_event, info, metin2::config::test_server() ? passes_per_sec(60) : passes_per_sec(5 * 60));

        set_status(STATUS_BATTLE);

        quest::quest_manager::instance().request_set_event_flag("battle_arena", map_index_);

        return true;
    }

    void battle_arena::end() {
        if (event_ != nullptr) {
            event_cancel(&event_);
        }
        force_end_ = false;
        map_index_ = 0;
        empire_ = 0;
        status_ = STATUS_CLOSE;

        quest::quest_manager::instance().request_set_event_flag("battle_arena", 0);
    }

    void battle_arena::force_end() {
        if (force_end_ == true) return;

        force_end_ = true;

        if (event_ != nullptr) {
            event_cancel(&event_);
        }

        auto *info = alloc_event_info<battle_arena_info>();

        info->map_index = map_index_;
        info->empire = empire_;
        info->state = 3;

        event_create(battle_arena_event, info, passes_per_sec(5));
    }

    void battle_arena::spawn_random_stone() {
        static const uint32_t vnum[7] = {8012, 8013, 8014, 8024, 8025, 8026, 8027};

        static const int region_info[3][4] = {
            {688900, 247600, 692700, 250000},
            {740200, 251000, 744200, 247700},
            {791400, 250900, 795900, 250400}
        };

        int idx = map_index_ - 190;
        if (idx < 0 || idx >= 3) return;

        character_manager::instance().spawn_mob_range(
            vnum[number(0, 6)],
            map_index_,
            region_info[idx][0], region_info[idx][1], region_info[idx][2], region_info[idx][3],
            false, true);
    }

    void battle_arena::spawn_last_boss() {
        static const uint32_t vnum = 8616;

        static const int position[3][2] = {
            {691000, 248900},
            {742300, 249000},
            {793600, 249300}
        };

        int idx = map_index_ - 190;
        if (idx < 0 || idx >= 3) return;

        character_manager::instance().spawn_mob(vnum, map_index_, position[idx][0], position[idx][1], 0);
    }
}
