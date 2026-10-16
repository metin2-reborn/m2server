#include <metin2/config/config.h>
#include <metin2/game/empire/threeway_war.h>

#include <metin2/game/admin/cmd.h>
#include <metin2/game/app/config.h>
#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/empire/start_position.h>
#include <metin2/game/persistence/log.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/quest/questlua.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/text/locale_service.h>
#include <metin2/game/world/regen.h>
#include <metin2/game/world/sectree_manager.h>

#include <metin2/core/utils.h>
#include <metin2/log/log.h>
#include <metin2/proto/character_types.h>
#include <metin2/proto/limits.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <format>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace metin2::game::inline empire {

    EVENTINFO(regen_mob_event_info) {
        uint32_t map_index;

        regen_mob_event_info()
            : map_index(0) {
        }
    };

    EVENTFUNC(regen_mob_event) {
        auto *info = dynamic_cast<regen_mob_event_info *>(event->info.get());

        if (info == nullptr) {
            metin2::log::error("Three-way war regen event has no info");
            return 0;
        }

        int map_index = info->map_index;

        std::string filename;
        std::string filename_value(get_sungzi_map_path());

        int choice = quest::quest_manager::instance().event_flag("threeway_war_choice");
        if (0 == choice)
            filename = filename_value + "regen00.txt";
        else {
            filename = filename_value + "regen00_" + std::to_string(choice) + ".txt";
        }

        sectree_map *map = sectree_manager::instance().map(map_index);

        if (nullptr != map) {
            if (!regen_load_in_file(filename.c_str(), map_index, map->setting.base_x, map->setting.base_y) && 0 != choice) {
                filename = filename_value + "regen00.txt";
                regen_load_in_file(filename.c_str(), map_index, map->setting.base_x, map->setting.base_y);
            }
        }

        return 0;
    }

    three_way_war::three_way_war() {
        initialize();
    }

    three_way_war::~three_way_war() {
        register_user_map_.clear();
        revive_token_map_.clear();
    }

    void three_way_war::initialize() {
        regen_flag_ = 0;

        memset(kill_score_, 0, sizeof(kill_score_));

        register_user_map_.clear();
        revive_token_map_.clear();
    }

    auto three_way_war::kill_score(uint8_t empire) const -> int {
        if (empire <= 0 || empire >= EMPIRE_MAX_NUM) {
            metin2::log::error("Three-way war kill score requested for invalid empire {:d}", empire);
            return 0;
        }

        return kill_score_[empire - 1];
    }

    void three_way_war::set_kill_score(uint8_t empire, int count) {
        if (empire <= 0 || empire >= EMPIRE_MAX_NUM) {
            metin2::log::error("Three-way war kill score set for invalid empire {:d}", empire);
            return;
        }

        kill_score_[empire - 1] = count;
    }

    void three_way_war::set_revive_token_for_player(uint32_t player_id, int count) {
        if (0 == player_id)
            return;

        revive_token_map_[player_id] = count;
    }

    auto three_way_war::revive_token_for_player(uint32_t player_id) -> int {
        if (0 == player_id)
            return 0;

        return revive_token_map_[player_id];
    }

    void three_way_war::decrease_revive_token_for_player(uint32_t player_id) {
        if (0 == player_id)
            return;

        revive_token_map_[player_id] = revive_token_map_[player_id] - 1;
    }

    auto three_way_war::load_setting(const char *file_name) -> bool {
        const std::string path = std::format("{}/{}", locale_service_get_base_path(), file_name);

        FILE *pf = fopen(path.c_str(), "r");

        if (nullptr == pf) {
            metin2::log::error("Cannot open three-way war file {}", path);
            return false;
        }

        char line[256];
        char sungzi_name[128];
        char pass_name[3][128];

        while (nullptr != fgets(line, 256, pf)) {
            if (0 == strncmp(line, "sungzi:", 7)) {
                struct forked_sungzi_map_info sungziinfo;

                sscanf(line + 7, "%d %d %d %d %d %d %d %127s %d",
                    &sungziinfo.forked_sung,
                    &sungziinfo.forked_sungzi_start_position[0][0], &sungziinfo.forked_sungzi_start_position[0][1],
                    &sungziinfo.forked_sungzi_start_position[1][0], &sungziinfo.forked_sungzi_start_position[1][1],
                    &sungziinfo.forked_sungzi_start_position[2][0], &sungziinfo.forked_sungzi_start_position[2][1],
                    sungzi_name, &sungziinfo.boss_mob_vnum);

                sungziinfo.map_name = static_cast<std::string>(sungzi_name);

                sung_zi_info_map_.push_back(sungziinfo);

                map_index_set_.insert(sungziinfo.forked_sung);
            } else if (0 == strncmp(line, "pass:", 5)) {
                struct forked_pass_map_info passinfo;

                sscanf(line + 5, "%d %d %d %127s %d %d %d %127s %d %d %d %127s",
                    &passinfo.forked_pass[0],
                    &passinfo.forked_pass_start_position[0][0], &passinfo.forked_pass_start_position[0][1], pass_name[0],
                    &passinfo.forked_pass[1],
                    &passinfo.forked_pass_start_position[1][0], &passinfo.forked_pass_start_position[1][1], pass_name[1],
                    &passinfo.forked_pass[2],
                    &passinfo.forked_pass_start_position[2][0], &passinfo.forked_pass_start_position[2][1], pass_name[2]);

                passinfo.map_name[0] = static_cast<std::string>(pass_name[0]);
                passinfo.map_name[1] = static_cast<std::string>(pass_name[1]);
                passinfo.map_name[2] = static_cast<std::string>(pass_name[2]);

                pass_info_map_.push_back(passinfo);

                map_index_set_.insert(passinfo.forked_pass[0]);
                map_index_set_.insert(passinfo.forked_pass[1]);
                map_index_set_.insert(passinfo.forked_pass[2]);
            }
        }

        fclose(pf);

        return true;
    }

    auto three_way_war::event_pass_map_info() const -> const forked_pass_map_info & {
        const size_t idx = quest::quest_manager::instance().event_flag("threeway_war_pass_idx");

        return pass_info_map_[idx];
    }

    auto three_way_war::event_sung_zi_map_info() const -> const forked_sungzi_map_info & {
        const size_t idx = quest::quest_manager::instance().event_flag("threeway_war_sungzi_idx");

        return sung_zi_info_map_[idx];
    }

    auto three_way_war::is_three_way_war_map_index(int map_index) const -> bool {
        return map_index_set_.find(map_index) != map_index_set_.end();
    }

    auto three_way_war::is_sung_zi_map_index(int map_index) const -> bool {
        auto it = sung_zi_info_map_.begin();

        for (; it != sung_zi_info_map_.end(); ++it) {
            if (map_index == it->forked_sung) {
                return true;
            }
        }

        return false;
    }

    void three_way_war::random_event_map_set() {
        const size_t pass_idx = number(0, pass_info_map_.size() - 1);
        const size_t sung_idx = number(0, sung_zi_info_map_.size() - 1);

        quest::quest_manager::instance().request_set_event_flag("threeway_war_sungzi_idx", sung_idx);
        quest::quest_manager::instance().request_set_event_flag("threeway_war_pass_idx", pass_idx);
    }

    auto three_way_war::is_registered_user(uint32_t player_id) const -> bool {
        auto iter = register_user_map_.find(player_id);

        if (iter == register_user_map_.end()) {
            return false;
        }

        return true;
    }

    void three_way_war::register_user(uint32_t player_id) {
        if (0 == player_id)
            return;

        register_user_map_.insert(std::make_pair(player_id, player_id));
    }

    auto get_kill_value(int level) -> int {
        int min_level_for1_point, max_level_for1_point;
        int min_level_for2_point, max_level_for2_point;
        int min_level_for3_point, max_level_for3_point;

        min_level_for1_point = 30;
        max_level_for1_point = 39;
        min_level_for2_point = 40;
        max_level_for2_point = 49;
        min_level_for3_point = 50;
        max_level_for3_point = 99;

        if (min_level_for1_point <= level && level <= max_level_for1_point) {
            return 1;
        } else if (min_level_for2_point <= level && level <= max_level_for2_point) {
            return 2;
        } else if (min_level_for3_point <= level && level <= max_level_for3_point) {
            return 3;
        }

        return 0;
    }

    void three_way_war::on_dead(character *dead, character *killer) {
        if (false == dead->is_pc())
            return;

        if (GM_PLAYER != dead->gm_level() && false == metin2::config::test_server())
            return;

        if (-1 == regen_flag())
            return;

        decrease_revive_token_for_player(dead->player_id());

        if (false == is_sung_zi_map_index(dead->map_index()))
            return;

        if (nullptr == killer || true != killer->is_pc())
            return;

        if (dead->empire() == killer->empire())
            return;

        int kill_score_value = kill_score(killer->empire());

        // A score of -1 means the empire is eliminated.
        if (kill_score_value >= 0) {
            kill_score_value += get_kill_value(dead->level());
            set_kill_score(killer->empire(), kill_score_value);
        }

        if (kill_score_value != 0 && (metin2::config::test_server() || (kill_score_value % 5) == 0)) {
            char buf_value[64 + 1];

            snprintf(buf_value, sizeof(buf_value), lc_text("현재 스코어 신수국:%d 천조국:%d 진노국:%d"),
                kill_score(1), kill_score(2), kill_score(3));

            send_notice_map(buf_value, get_sungzi_map_index(), false);
        }

        const int victory_score = quest::quest_manager::instance().event_flag("threeway_war_kill_count");

        if (0 == regen_flag()) {
            int eliminated_empire_count = 0;
            uint8_t lose_empire = 0;

            for (int n = 1; n < 4; ++n) {
                if (victory_score > kill_score(n)) {
                    ++eliminated_empire_count;
                    lose_empire = n;
                }
            }

            if (1 != eliminated_empire_count)
                return;

            set_kill_score(1, 0);
            set_kill_score(2, 0);
            set_kill_score(3, 0);
            set_kill_score(lose_empire, -1);

            quest::warp_all_to_map_my_empire_event_info *info;

            info = alloc_event_info<quest::warp_all_to_map_my_empire_event_info>();

            info->map_index_from = get_sungzi_map_index();
            info->map_index_to = empire_start_map(lose_empire);
            info->x = empire_start_x(lose_empire);
            info->y = empire_start_y(lose_empire);
            info->empire = lose_empire;

            event_create(quest::warp_all_to_map_my_empire_event, info, passes_per_sec(10));

            info = alloc_event_info<quest::warp_all_to_map_my_empire_event_info>();

            info->map_index_from = get_pass_map_index(lose_empire);
            info->map_index_to = empire_start_map(lose_empire);
            info->x = empire_start_x(lose_empire);
            info->y = empire_start_y(lose_empire);
            info->empire = lose_empire;

            event_create(quest::warp_all_to_map_my_empire_event, info, passes_per_sec(10));

            const std::string nation(empire_name(lose_empire));
            const std::string script(
                lc_text("성지의 마왕: 너희 ") +
                nation +
                lc_text("녀석들은 이곳 성지에 있을 자격을 잃었다. 모두 성지에서 물러나거라~~[ENTER][ENTER] 10초 후에 모두 마을로 이동하게 됩니다. ") +
                "[ENTER][DONE]");

            character_manager::instance().send_script_to_map(dead->map_index(), script);

            char notice[512 + 1];
            snprintf(notice, sizeof(notice), lc_text("삼거리 전투에서 %s 국가가 가장먼저 탈락을 하였습니다"), nation.c_str());
            broadcast_notice(notice);

            const std::string log_text = std::format("First Step: {} exclusion", nation);
            log_manager::instance().char_log(0, 0, 0, 0, "THREEWAY", log_text.c_str(), nullptr);

            auto *regen_info = alloc_event_info<regen_mob_event_info>();

            regen_info->map_index = dead->map_index();

            event_create(regen_mob_event, regen_info, passes_per_sec(10));

            set_regen_flag(1);
        } else if (1 == regen_flag()) {
            int victory_empire_index = 0;

            for (int n = 1; n < 4; ++n) {
                kill_score_value = kill_score(n);

                if (kill_score_value == -1)
                    continue;

                if (victory_score <= kill_score_value) {
                    victory_empire_index = n;
                    break;
                }
            }

            if (0 == victory_empire_index)
                return;

            for (int n = 1; n < 4; ++n) {
                if (n != victory_empire_index) {
                    uint8_t lose_empire = n;
                    quest::warp_all_to_map_my_empire_event_info *info;

                    info = alloc_event_info<quest::warp_all_to_map_my_empire_event_info>();

                    info->map_index_from = get_sungzi_map_index();
                    info->map_index_to = empire_start_map(lose_empire);
                    info->x = empire_start_x(lose_empire);
                    info->y = empire_start_y(lose_empire);
                    info->empire = lose_empire;

                    event_create(quest::warp_all_to_map_my_empire_event, info, passes_per_sec(5));

                    info = alloc_event_info<quest::warp_all_to_map_my_empire_event_info>();

                    info->map_index_from = get_pass_map_index(lose_empire);
                    info->map_index_to = empire_start_map(lose_empire);
                    info->x = empire_start_x(lose_empire);
                    info->y = empire_start_y(lose_empire);
                    info->empire = lose_empire;

                    event_create(quest::warp_all_to_map_my_empire_event, info, passes_per_sec(5));
                }
            }

            {
                char buf_value[64 + 1];
                snprintf(buf_value, sizeof(buf_value), lc_text("현재 스코어 신수국:%d 천조국:%d 진노국:%d"),
                    kill_score(1), kill_score(2), kill_score(3));

                send_notice_map(buf_value, get_sungzi_map_index(), false);
            }

            sectree_map *sec_map = sectree_manager::instance().map(dead->map_index());

            if (nullptr != sec_map) {
                const std::string empire_name_value(empire_name(victory_empire_index));
                const std::string script(
                    empire_name_value +
                    lc_text(". 너희가 성지의 수호자를 잡게 된다면 너희는 성지의 주인이 된다.[ENTER][ENTER] ") +
                    "[ENTER][DONE]");

                struct packet_script pack_script;

                pack_script.header = HEADER_GC_SCRIPT;
                pack_script.skin = 1;
                pack_script.src_size = script.size();

                temp_buffer buf;
                pack_script.size = pack_script.src_size + sizeof(struct packet_script);
                buf.write(&pack_script, sizeof(struct packet_script));
                buf.write(&script[0], script.size());

                quest::send_buffer_to_map(sec_map, buf, victory_empire_index);

                const std::string log_text = std::format("Second Step: {} remain", empire_name(victory_empire_index));
                log_manager::instance().char_log(0, 0, 0, 0, "THREEWAY", log_text.c_str(), nullptr);
            }

            for (int n = 0; n < quest::quest_manager::instance().event_flag("threeway_war_boss_count");) {
                int x = dead->x();
                int y = dead->y();

                x = prob(50) ? x - number(200, 1000) : x + number(200, 1000);
                y = prob(50) ? y - number(200, 1000) : y + number(200, 1000);

                if (x < 0)
                    x = dead->x();

                if (y < 0)
                    y = dead->y();

                character *ch = character_manager::instance().spawn_mob(
                    event_sung_zi_map_info().boss_mob_vnum,
                    dead->map_index(),
                    x, y, 0, false);

                if (nullptr != ch) {
                    ch->set_aggressive();
                    ++n;
                }
            }

            set_regen_flag(-1);
        }
    }

    void three_way_war::remove_all_monsters_in_three_way() const {
        auto iter = map_index_set_.begin();

        while (iter != map_index_set_.end()) {
            sectree_map *sec_map = sectree_manager::instance().map(*iter);

            if (nullptr != sec_map) {
                sec_map->for_each([](entity *ent) {
                    if (true == ent->is_type(ENTITY_CHARACTER)) {
                        auto *ch = static_cast<character *>(ent);

                        if (false == ch->is_pc()) {
                            ch->dead();
                        }
                    }
                });
            }

            ++iter;
        }
    }

    auto get_sungzi_map_path() -> const char * {
        static std::string map_path;

        map_path = std::format("{}/map/{}/",
            locale_service_get_base_path(),
            three_way_war::instance().event_sung_zi_map_info().map_name);

        return map_path.c_str();
    }

    auto get_pass_map_path(uint8_t empire) -> const char * {
        if (empire > 0 && empire < EMPIRE_MAX_NUM) {
            static std::string map_path;

            map_path = std::format("{}/map/{}/",
                locale_service_get_base_path(),
                three_way_war::instance().event_pass_map_info().map_name[empire - 1]);

            return map_path.c_str();
        }

        return nullptr;
    }

    auto get_pass_map_index(uint8_t empire) -> int {
        if (empire > 0 && empire < EMPIRE_MAX_NUM) {
            return three_way_war::instance().event_pass_map_info().forked_pass[empire - 1];
        }

        return 0;
    }

    auto get_sungzi_map_index() -> int {
        return three_way_war::instance().event_sung_zi_map_info().forked_sung;
    }

    auto get_sungzi_start_x(uint8_t empire) -> int {
        if (empire > 0 && empire < EMPIRE_MAX_NUM) {
            return three_way_war::instance().event_sung_zi_map_info().forked_sungzi_start_position[empire - 1][0];
        }

        return 0;
    }

    auto get_sungzi_start_y(uint8_t empire) -> int {
        if (empire > 0 && empire < EMPIRE_MAX_NUM) {
            return three_way_war::instance().event_sung_zi_map_info().forked_sungzi_start_position[empire - 1][1];
        }

        return 0;
    }
}
