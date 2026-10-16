#include <metin2/db/social/marriage.h>

#include <metin2/db/clients/client_manager.h>
#include <metin2/db/clients/peer.h>
#include <metin2/db/storage/db_manager.h>

#include <metin2/common/utils.h>
#include <metin2/log/log.h>
#include <metin2/proto/tables.h>
#include <metin2/sql/async_sql.h>
#include <metin2/sql/query.h>

#include <mysql.h>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include <sys/types.h>

namespace metin2::db::inline social {

    namespace marriage {
        const uint32_t WEDDING_LENGTH = 60 * 60;

        auto operator>(const wedding &lhs, const wedding &rhs) -> bool {
            return lhs.time > rhs.time;
        }

        auto operator>(const wedding_info &lhs, const wedding_info &rhs) -> bool {
            return lhs.time > rhs.time;
        }

        manager::manager() = default;

        auto manager::initialize() -> bool {
            const std::string query = sql::format(
                "SELECT pid1, pid2, love_point, time, is_married, p1.name, p2.name FROM marriage, player as p1, player as p2 WHERE p1.id = pid1 AND p2.id = pid2");

            auto pmsg_delete = get_db_manager().direct_query("DELETE FROM marriage WHERE is_married = 0");
            auto pmsg = get_db_manager().direct_query(query);

            sql_result *res = pmsg->get();
            metin2::log::info("Marriage: loading {:d} marriages", res->num_rows);

            if (res->num_rows > 0) {
                for (uint row_value = 0; row_value != res->num_rows; ++row_value) {
                    MYSQL_ROW row = mysql_fetch_row(res->res.get());

                    uint32_t pid1 = 0;
                    str_to_number(pid1, row[0]);
                    uint32_t pid2 = 0;
                    str_to_number(pid2, row[1]);
                    int love_point = 0;
                    str_to_number(love_point, row[2]);
                    uint32_t time = 0;
                    str_to_number(time, row[3]);
                    uint8_t is_married = 0;
                    str_to_number(is_married, row[4]);
                    const char *name1 = row[5];
                    const char *name2 = row[6];

                    marriage *marriage_value = marriages_.emplace_back(std::make_unique<marriage>(pid1, pid2, love_point, time, is_married, name1, name2)).get();
                    marriage_by_pid_.insert(std::make_pair(pid1, marriage_value));
                    marriage_by_pid_.insert(std::make_pair(pid2, marriage_value));

                    metin2::log::debug("Marriage: row {:d}, love points {:d}, time {:d}, married {:d}, {:d}:{} and {:d}:{}", row_value, love_point, time, is_married, pid1, name1, pid2, name2);
                }
            }
            return true;
        }

        auto manager::get(uint32_t player_id) -> marriage * {
            auto it = marriage_by_pid_.find(player_id);

            if (it != marriage_by_pid_.end())
                return it->second;

            return nullptr;
        }

        void align(uint32_t &pid1, uint32_t &pid2) {
            if (pid1 > pid2)
                std::swap(pid1, pid2);
        }

        void manager::add(uint32_t pid1, uint32_t pid2, const char *name1_value, const char *name2_value) {
            uint32_t now = get_client_manager().current_time();
            if (is_married(pid1) || is_married(pid2)) {
                metin2::log::warn("Marriage: pid {:d} or pid {:d} is already married", pid1, pid2);
                return;
            }

            align(pid1, pid2);

            const std::string query = sql::format("INSERT INTO marriage(pid1, pid2, love_point, time) VALUES ({}, {}, 0, {})", pid1, pid2, now);

            auto pmsg = get_db_manager().direct_query(query);

            sql_result *res = pmsg->get();
            if (res->affected_rows == 0 || res->affected_rows == (uint32_t)-1) {
                metin2::log::error("Marriage: cannot insert marriage of pid {:d} and pid {:d}", pid1, pid2);
                return;
            }

            metin2::log::info("Marriage: added marriage of pid {:d} and pid {:d}", pid1, pid2);

            marriage *marriage_value = marriages_.emplace_back(std::make_unique<marriage>(pid1, pid2, 0, now, 0, name1_value, name2_value)).get();
            marriage_by_pid_.insert(std::make_pair(pid1, marriage_value));
            marriage_by_pid_.insert(std::make_pair(pid2, marriage_value));

            packet_marriage_add p;
            p.pid1 = pid1;
            p.pid2 = pid2;
            p.t_marry_time = now;
            strlcpy(p.name1, name1_value, sizeof(p.name1));
            strlcpy(p.name2, name2_value, sizeof(p.name2));
            get_client_manager().forward_packet(HEADER_DG_MARRIAGE_ADD, &p, sizeof(p));
        }

        void manager::update(uint32_t pid1_value, uint32_t pid2_value, int32_t love_point_value, uint8_t married_value) {
            marriage *marriage_value = get(pid1_value);
            if (!marriage_value || marriage_value->other(pid1_value) != pid2_value) {
                metin2::log::warn("Marriage: update for pid {:d} and pid {:d}, who are not married to each other", pid1_value, pid2_value);
                return;
            }

            align(pid1_value, pid2_value);

            const std::string query = sql::format("UPDATE marriage SET love_point = {}, is_married = {} WHERE pid1 = {} AND pid2 = {}",
                love_point_value, married_value, marriage_value->pid1, marriage_value->pid2);

            auto pmsg = get_db_manager().direct_query(query);

            sql_result *res = pmsg->get();
            if (res->affected_rows == 0 || res->affected_rows == (uint32_t)-1) {
                metin2::log::error("Marriage: cannot update marriage of pid {:d} and pid {:d}", pid1_value, pid2_value);
                return;
            }

            metin2::log::info("Marriage: updated pid {:d} and pid {:d}, love points {:d}, married {:d}", pid1_value, pid2_value, love_point_value, married_value);
            marriage_value->love_point = love_point_value;
            marriage_value->is_married = married_value;

            packet_marriage_update p;
            p.pid1 = pid1_value;
            p.pid2 = pid2_value;
            p.love_point = marriage_value->love_point;
            p.married = marriage_value->is_married;
            get_client_manager().forward_packet(HEADER_DG_MARRIAGE_UPDATE, &p, sizeof(p));
        }

        void manager::remove(uint32_t pid1_value, uint32_t pid2_value) {
            marriage *marriage_value = get(pid1_value);

            if (marriage_value) {
                metin2::log::debug("Marriage: removing pid {:d} and pid {:d} (recorded partner {:d})", pid1_value, pid2_value, marriage_value->other(pid1_value));
            }
            if (!marriage_value || marriage_value->other(pid1_value) != pid2_value) {
                auto it = marriage_by_pid_.begin();

                for (; it != marriage_by_pid_.end(); ++it) {
                    metin2::log::debug("Marriage: existing marriage pid {:d} and pid {:d}", it->second->pid1, it->second->pid2);
                }
                metin2::log::warn("Marriage: removal for pid {:d} and pid {:d}, who are not married to each other", pid1_value, pid2_value);
                return;
            }

            align(pid1_value, pid2_value);

            const std::string query = sql::format("DELETE FROM marriage WHERE pid1 = {} AND pid2 = {}", pid1_value, pid2_value);

            auto pmsg = get_db_manager().direct_query(query);

            sql_result *res = pmsg->get();
            if (res->affected_rows == 0 || res->affected_rows == (uint32_t)-1) {
                metin2::log::error("Marriage: cannot delete marriage of pid {:d} and pid {:d}", pid1_value, pid2_value);
                return;
            }

            metin2::log::info("Marriage: pid {:d} and pid {:d} divorced", pid1_value, pid2_value);

            marriage_by_pid_.erase(pid1_value);
            marriage_by_pid_.erase(pid2_value);

            packet_marriage_remove p;
            p.pid1 = pid1_value;
            p.pid2 = pid2_value;
            get_client_manager().forward_packet(HEADER_DG_MARRIAGE_REMOVE, &p, sizeof(p));

            std::erase_if(marriages_, [marriage_value](const auto &m) { return m.get() == marriage_value; });
        }

        void manager::on_setup(peer *peer) {
            for (const auto &owned : marriages_) {
                marriage *marriage = owned.get();

                {
                    packet_marriage_add p;
                    p.pid1 = marriage->pid1;
                    p.pid2 = marriage->pid2;
                    p.t_marry_time = marriage->time;
                    strlcpy(p.name1, marriage->name1.c_str(), sizeof(p.name1));
                    strlcpy(p.name2, marriage->name2.c_str(), sizeof(p.name2));
                    peer->encode_header(HEADER_DG_MARRIAGE_ADD, 0, sizeof(p));
                    peer->encode(&p, sizeof(p));
                }

                {
                    packet_marriage_update p;
                    p.pid1 = marriage->pid1;
                    p.pid2 = marriage->pid2;
                    p.love_point = marriage->love_point;
                    p.married = marriage->is_married;
                    peer->encode_header(HEADER_DG_MARRIAGE_UPDATE, 0, sizeof(p));
                    peer->encode(&p, sizeof(p));
                }
            }

            for (auto it = running_wedding_.begin(); it != running_wedding_.end(); ++it) {
                const wedding &t = it->second;

                packet_wedding_ready p;
                p.pid1 = t.pid1;
                p.pid2 = t.pid2;
                p.map_index = t.map_index;

                peer->encode_header(HEADER_DG_WEDDING_READY, 0, sizeof(p));
                peer->encode(&p, sizeof(p));

                packet_wedding_start p2;
                p2.pid1 = t.pid1;
                p2.pid2 = t.pid2;

                peer->encode_header(HEADER_DG_WEDDING_START, 0, sizeof(p2));
                peer->encode(&p2, sizeof(p2));
            }
        }

        void manager::ready_wedding(uint32_t map_index, uint32_t pid1, uint32_t pid2) {
            uint32_t start_time = get_client_manager().current_time();
            pq_wedding_start_.push(wedding(start_time + 5, map_index, pid1, pid2));
        }

        void manager::end_wedding(uint32_t pid1, uint32_t pid2) {
            auto it = running_wedding_.find(std::make_pair(pid1, pid2));
            if (it == running_wedding_.end()) {
                metin2::log::warn("Marriage: cannot end wedding of pid {:d} and pid {:d}, no wedding running", pid1, pid2);
                return;
            }

            wedding &w = it->second;

            packet_wedding_end p;
            p.pid1 = w.pid1;
            p.pid2 = w.pid2;
            get_client_manager().forward_packet(HEADER_DG_WEDDING_END, &p, sizeof(p));
            running_wedding_.erase(it);
        }

        void manager::update() {
            uint32_t now = get_client_manager().current_time();

            if (!pq_wedding_end_.empty()) {
                while (!pq_wedding_end_.empty() && pq_wedding_end_.top().time <= now) {
                    wedding_info wi = pq_wedding_end_.top();
                    pq_wedding_end_.pop();

                    auto it = running_wedding_.find(std::make_pair(wi.pid1, wi.pid2));
                    if (it == running_wedding_.end())
                        continue;

                    const wedding w = it->second;

                    packet_wedding_end p;
                    p.pid1 = w.pid1;
                    p.pid2 = w.pid2;
                    get_client_manager().forward_packet(HEADER_DG_WEDDING_END, &p, sizeof(p));
                    running_wedding_.erase(it);

                    auto it_marriage = marriage_by_pid_.find(w.pid1);

                    if (it_marriage != marriage_by_pid_.end()) {
                        marriage *marriage = it_marriage->second;
                        if (!marriage->is_married) {
                            remove(marriage->pid1, marriage->pid2);
                        }
                    }
                }
            }
            if (!pq_wedding_start_.empty()) {
                while (!pq_wedding_start_.empty() && pq_wedding_start_.top().time <= now) {
                    wedding w = pq_wedding_start_.top();
                    pq_wedding_start_.pop();

                    packet_wedding_start p;
                    p.pid1 = w.pid1;
                    p.pid2 = w.pid2;
                    get_client_manager().forward_packet(HEADER_DG_WEDDING_START, &p, sizeof(p));

                    w.time += WEDDING_LENGTH;
                    pq_wedding_end_.push(wedding_info(w.time, w.pid1, w.pid2));
                    running_wedding_.insert(make_pair(std::make_pair(w.pid1, w.pid2), w));
                }
            }
        }
    }
}
