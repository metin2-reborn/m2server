#pragma once

#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>

#include <metin2/common/singleton.h>

#include <cstdint>
#include <ctime>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>

namespace metin2::game::inline social {

    namespace marriage {
        struct wedding_info {
            uint32_t map_index;
        };

        extern const int MARRIAGE_POINT_PER_DAY;
        struct marriage {
            uint32_t pid1;
            uint32_t pid2;
            int love_point;
            time_t marry_time;
            character *ch1;
            character *ch2;
            bool needs_save;
            bool is_married;
            std::string name1;
            std::string name2;

            std::unique_ptr<game::social::marriage::wedding_info> wedding_info;

            marriage(uint32_t pid1_value, uint32_t pid2_value, int love_point_value, time_t marry_time_value, const char *name1, const char *name2) : pid1(pid1_value),
                                                                                                                                                      pid2(pid2_value),
                                                                                                                                                      love_point(love_point_value),
                                                                                                                                                      marry_time(marry_time_value),
                                                                                                                                                      is_married(false),
                                                                                                                                                      name1(name1),
                                                                                                                                                      name2(name2),
                                                                                                                                                      event_near_check(nullptr) {
                ch1 = ch2 = nullptr;
                needs_save = false;
                is_last_near = false;
                last_love_point = 0;
            }

            ~marriage();

            void login(character *ch);
            void logout(uint32_t pid);

            auto is_online() -> bool {
                return ch1 && ch2;
            }

            auto is_near() -> bool;

            auto other(uint32_t pid) const -> uint32_t {
                if (pid1 == pid)
                    return pid2;

                if (pid2 == pid)
                    return pid1;

                return 0;
            }

            auto marriage_point() -> int;
            auto marriage_grade() -> int;

            auto bonus(uint32_t item_vnum, bool share = true, character *me = nullptr) -> int;

            void warp_to_wedding_map(uint32_t pid);
            void save();
            void set_married();

            void update(uint32_t point);
            void request_end_wedding();

            void start_near_check_event();
            void stop_near_check_event();
            void near_check();

            bool is_last_near;
            uint8_t last_love_point;
            event_ptr event_near_check;
        };

        class manager : public singleton<manager> {
          public:
            manager();
            ~manager() override;

            auto initialize() -> bool;
            void destroy();

            auto get(uint32_t player_id) -> marriage *;

            auto is_marriage_unique_item(uint32_t item_vnum) -> bool;

            auto is_married(uint32_t player_id) -> bool;
            auto is_engaged(uint32_t player_id) -> bool;
            auto is_engaged_or_married(uint32_t player_id) -> bool;

            void request_add(uint32_t pid1, uint32_t pid2, const char *name1_value, const char *name2_value);
            void add(uint32_t pid1, uint32_t pid2, time_t t_marry_time, const char *name1, const char *name2);

            void request_update(uint32_t pid1, uint32_t pid2, int update_point, uint8_t married_value);
            void update(uint32_t pid1, uint32_t pid2, int32_t total_point, uint8_t married);

            void request_remove(uint32_t pid1, uint32_t pid2);
            void remove(uint32_t pid1, uint32_t pid2);

            void login(character *ch);

            void logout(uint32_t pid);
            void logout(character *ch);

            void wedding_ready(uint32_t pid1, uint32_t pid2, uint32_t map_index_value);
            void wedding_start(uint32_t pid1, uint32_t pid2);
            void wedding_end(uint32_t pid1, uint32_t pid2);

            void request_end_wedding(uint32_t pid1, uint32_t pid2);

            template <typename Func>
            auto for_each_wedding(Func f) -> Func;

          private:
            std::unordered_map<marriage *, std::unique_ptr<marriage>> marriages_;
            std::map<uint32_t, marriage *> marriage_by_pid_;
            std::set<std::pair<uint32_t, uint32_t>> wedding_;
        };

        template <typename Func>
        auto manager::for_each_wedding(Func f) -> Func {
            for (auto it = wedding_.begin(); it != wedding_.end(); ++it) {
                marriage *pMarriage = get(it->first);
                if (pMarriage)
                    f(pMarriage);
            }
            return f;
        }

    }
}
