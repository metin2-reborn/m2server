#include <metin2/db/empire/monarch.h>

#include <metin2/db/app/main.h>
#include <metin2/db/storage/db_manager.h>

#include <metin2/common/utils.h>
#include <metin2/config/config.h>
#include <metin2/log/log.h>
#include <metin2/proto/tables.h>
#include <metin2/sql/async_sql.h>
#include <metin2/sql/query.h>

#include <mysql.h>

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace metin2::db::inline empire {

    monarch::monarch() {
        memset(&monarch_info_, 0, sizeof(monarch_info));
    }

    monarch::~monarch() {
    }

    auto monarch::vote_monarch(uint32_t pid, uint32_t selectdpid) -> bool {
        map_monarchelection::iterator it = map_monarch_election_.find(pid);

        if (it == map_monarch_election_.end()) {
            auto p = std::make_unique<monarch_election_info>();
            p->pid = pid;
            p->selectedpid = selectdpid;
            map_monarch_election_.insert(map_monarchelection::value_type(pid, std::move(p)));

            const std::string query = sql::format(
                "INSERT INTO monarch_election(pid, selectedpid, electiondata) VALUES({}, {}, now())", pid, selectdpid);

            db_manager::instance().async_query(query);
            return 1;
        }

        return 0;
    }

    void monarch::elect_monarch() {
        int size = vec_monarch_candidacy().size();

        auto s = std::make_unique<int[]>(size);

        auto it = map_monarch_election_.begin();

        int idx = 0;

        for (; it != map_monarch_election_.end(); ++it) {
            if ((idx = candidacy_index(it->second->pid)) < 0)
                continue;

            ++s[idx];

            if (metin2::config::test_server())
                metin2::log::debug("Monarch election: pid {:d} voted for candidate pid {:d}", it->second->pid, vec_monarch_candidacy_[idx].pid);
        }
    }

    auto monarch::is_candidacy(uint32_t pid) -> bool {
        vec_monarchcandidacy::iterator it = vec_monarch_candidacy_.begin();

        for (; it != vec_monarch_candidacy_.end(); ++it) {
            if (it->pid == pid)
                return false;
        }

        return true;
    }

    auto monarch::add_candidacy(uint32_t pid, const char *name) -> bool {
        if (is_candidacy(pid) == false)
            return false;

        monarch_candidacy info;

        info.pid = pid;
        strlcpy(info.name, name, sizeof(info.name));
        vec_monarch_candidacy_.push_back(info);

        const std::string query = sql::format("INSERT INTO monarch_candidacy(pid, date) VALUES({}, now())", pid);

        db_manager::instance().async_query(query);
        return true;
    }

    auto monarch::del_candidacy(const char *name) -> bool {
        auto it = vec_monarch_candidacy_.begin();
        for (; it != vec_monarch_candidacy_.end(); ++it) {
            if (0 == strncmp(it->name, name, sizeof(it->name))) {
                const std::string query = sql::format("DELETE FROM monarch_candidacy WHERE pid={} ", it->pid);
                db_manager::instance().async_query(query);

                vec_monarch_candidacy_.erase(it);
                return true;
            }
        }
        return false;
    }

    auto monarch::is_monarch(int empire, uint32_t pid) -> bool {
        if (monarch_info_.pid[empire] != pid)
            return false;
        return true;
    }

    auto monarch::add_money(int empire, int64_t money_value) -> bool {
        if (empire <= 0 || empire >= EMPIRE_MAX_NUM)
            return false;

        if (monarch_info_.money[empire] + money_value > 2000000000)
            return true;

        monarch_info_.money[empire] += money_value;

        int64_t Money64 = monarch_info_.money[empire];

        const std::string query = sql::format("UPDATE monarch set money={} where empire={}", Money64, empire);

        db_manager::instance().async_query(query);

        return true;
    }

    auto monarch::dec_money(int empire, int64_t money_value) -> bool {
        if (empire <= 0 || empire >= EMPIRE_MAX_NUM)
            return false;

        if (monarch_info_.money[empire] - money_value < 0)
            return false;
        monarch_info_.money[empire] -= money_value;

        int64_t Money64 = monarch_info_.money[empire];

        const std::string query = sql::format("UPDATE monarch set money={} where empire={}", Money64, empire);

        db_manager::instance().async_query(query);
        return true;
    }

    auto monarch::take_money(int empire, uint32_t pid, int64_t money_value) -> bool {
        if (empire <= 0 || empire >= EMPIRE_MAX_NUM)
            return false;

        if (is_monarch(empire, pid) == false)
            return false;

        if (monarch_info_.money[empire] < money_value)
            return false;

        monarch_info_.money[empire] -= money_value;

        const std::string query = sql::format("UPDATE monarch SET money={} WHERE empire={}", monarch_info_.money[empire], empire);

        db_manager::instance().async_query(query);

        if (metin2::config::test_server())
            metin2::log::info("Monarch: took money from empire {:d}, treasury now {:d}", empire, monarch_info_.money[empire]);
        return true;
    }

    auto monarch::load_monarch() -> bool {
        monarch_info *p = &monarch_info_;
        const std::string query = sql::format("SELECT empire, pid, name, money, windate FROM monarch a, player{} b where a.pid=b.id", sql::raw(get_table_postfix()));
        auto pMsg = db_manager::instance().direct_query(query, SQL_PLAYER);

        if (pMsg->get()->num_rows == 0) {
            return false;
        }

        MYSQL_ROW row;
        while ((row = mysql_fetch_row(pMsg->get()->res)) != nullptr) {
            int idx = 0;
            int Empire = 0;
            str_to_number(Empire, row[idx++]);

            str_to_number(p->pid[Empire], row[idx++]);
            strlcpy(p->name[Empire], row[idx++], sizeof(p->name[Empire]));

            str_to_number(p->money[Empire], row[idx++]);
            strlcpy(p->date[Empire], row[idx++], sizeof(p->date[Empire]));

            if (metin2::config::test_server())
                metin2::log::info("Monarch: loaded empire {:d} monarch pid {:d}, money {:d}, elected {}", Empire, p->pid[Empire], p->money[Empire], p->date[Empire]);
        }

        return true;
    }

    auto monarch::set_monarch(const char *name) -> bool {
        monarch_info *p = &monarch_info_;
        std::string query = sql::format(
            "SELECT b.empire, a.id, a.name FROM player{} AS a, player_index{} AS b WHERE a.account_id=b.id AND a.name={}",
            sql::raw(get_table_postfix()), sql::raw(get_table_postfix()), std::string_view(name, strnlen(name, CHARACTER_NAME_MAX_LEN)));
        auto pMsg = db_manager::instance().direct_query(query, SQL_PLAYER);

        if (pMsg->get()->num_rows == 0) {
            return false;
        }

        MYSQL_ROW row;
        int Empire = 0;
        while ((row = mysql_fetch_row(pMsg->get()->res)) != nullptr) {
            int idx = 0;
            str_to_number(Empire, row[idx++]);

            str_to_number(p->pid[Empire], row[idx++]);
            strlcpy(p->name[Empire], row[idx++], sizeof(p->name[Empire]));

            if (metin2::config::test_server())
                metin2::log::info("Monarch: set empire {:d} monarch pid {:d}, money {:d}, elected {}", Empire, p->pid[Empire], p->money[Empire], p->date[Empire]);
        }

        query = sql::format(
            "REPLACE INTO monarch (empire, pid, windate, money) VALUES({}, {}, NOW(), {})", Empire, p->pid[Empire], p->money[Empire]);

        db_manager::instance().async_query(query, SQL_PLAYER);
        return true;
    }

    auto monarch::del_monarch(const char *name) -> bool {
        for (int n = 1; n < 4; ++n) {
            if (0 == strncmp(monarch_info_.name[n], name, sizeof(monarch_info_.name[n]))) {
                int Empire = n;
                const std::string query = sql::format("DELETE FROM monarch WHERE empire={}", Empire);
                auto pMsg = db_manager::instance().direct_query(query, SQL_PLAYER);

                if (pMsg->get()->affected_rows == 0 || pMsg->get()->affected_rows == (uint32_t)-1) {
                    metin2::log::error("Cannot delete monarch, query affected no rows: {}", query);
                    return false;
                }

                memset(monarch_info_.name[Empire], 0, 32);
                monarch_info_.money[Empire] = 0;
                monarch_info_.pid[Empire] = 0;
                return true;
            }
        }

        return false;
    }

    auto monarch::candidacy_index(uint32_t pid) -> int {
        auto it = vec_monarch_candidacy_.begin();

        for (int n = 0; it != vec_monarch_candidacy_.end(); ++it, ++n) {
            if (it->pid == pid)
                return n;
        }

        return -1;
    }
}
