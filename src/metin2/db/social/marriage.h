#pragma once

#include <metin2/db/clients/peer.h>

#include <metin2/common/singleton.h>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace metin2::db::inline social {

    namespace marriage {
        struct wedding_info {
            uint32_t time;
            uint32_t pid1;
            uint32_t pid2;
            wedding_info(uint32_t time_value, uint32_t pid1, uint32_t pid2)
                : time(time_value),
                  pid1(pid1),
                  pid2(pid2) {
            }
        };

        struct wedding {
            uint32_t time;
            uint32_t map_index;
            uint32_t pid1;
            uint32_t pid2;

            wedding(uint32_t time_value, uint32_t map_index_value, uint32_t pid1, uint32_t pid2)
                : time(time_value),
                  map_index(map_index_value),
                  pid1(pid1),
                  pid2(pid2) {
            }
        };

        extern auto operator>(const wedding &lhs, const wedding &rhs) -> bool;
        extern auto operator>(const wedding_info &lhs, const wedding_info &rhs) -> bool;

        struct marriage {
            uint32_t pid1;
            uint32_t pid2;
            int love_point;
            uint32_t time;
            uint8_t is_married;
            std::string name1;
            std::string name2;

            marriage(uint32_t pid1_value, uint32_t pid2_value, int love_point_value, uint32_t time_value, uint8_t is_married_value, const char *name1, const char *name2)
                : pid1(pid1_value), pid2(pid2_value), love_point(love_point_value), time(time_value), is_married(is_married_value), name1(name1), name2(name2) {
            }

            auto other(uint32_t pid) -> uint32_t {
                if (pid1 == pid)
                    return pid2;

                if (pid2 == pid)
                    return pid1;

                return 0;
            }
        };

        class manager : public singleton<manager> {
          public:
            manager();

            auto initialize() -> bool;

            auto get(uint32_t player_id) -> marriage *;
            auto is_married(uint32_t player_id) -> bool {
                return get(player_id) != nullptr;
            }

            void add(uint32_t pid1, uint32_t pid2, const char *name1_value, const char *name2_value);
            void remove(uint32_t pid1_value, uint32_t pid2_value);
            void update(uint32_t pid1_value, uint32_t pid2_value, int32_t love_point_value, uint8_t married_value);

            void ready_wedding(uint32_t map_index, uint32_t pid1, uint32_t pid2);
            void end_wedding(uint32_t pid1, uint32_t pid2);

            void on_setup(peer *peer);

            void update();

          private:
            std::vector<std::unique_ptr<marriage>> marriages_;
            std::map<uint32_t, marriage *> marriage_by_pid_;

            std::priority_queue<wedding, std::vector<wedding>, std::greater<wedding>> pq_wedding_start_;

            std::priority_queue<wedding_info, std::vector<wedding_info>, std::greater<wedding_info>> pq_wedding_end_;

            std::map<std::pair<uint32_t, uint32_t>, wedding> running_wedding_;
        };
    }
}
