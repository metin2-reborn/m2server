#include <metin2/db/storage/db_manager.h>

#include <metin2/db/clients/client_manager.h>

#include <metin2/log/log.h>
#include <metin2/sql/async_sql.h>

#include <mysql.h>

#include <cassert>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace metin2::db::inline storage {

    static constexpr const char CONNECTION_CHARSET[] = "latin1";

    db_manager::~db_manager() {
        clear();
    }

    void db_manager::clear() {
        for (int i = 0; i < SQL_MAX_NUM; ++i) {
            main_sql_[i].reset();
            direct_sql_[i].reset();
            async_sql_[i].reset();
        }
    }

    void db_manager::quit() {
        for (int i = 0; i < SQL_MAX_NUM; ++i) {
            if (main_sql_[i])
                main_sql_[i]->quit();

            if (async_sql_[i])
                async_sql_[i]->quit();

            if (direct_sql_[i])
                direct_sql_[i]->quit();
        }
    }

    auto db_manager::pop_result() -> std::unique_ptr<sql_msg> {
        for (int i = 0; i < SQL_MAX_NUM; ++i)
            if (main_sql_[i])
                if (auto msg = main_sql_[i]->pop_result())
                    return msg;

        return nullptr;
    }

    auto db_manager::connect(int slot, const char *db_address, const int db_port, const char *db_name, const char *user, const char *pwd) -> int {
        if (db_address == nullptr || db_name == nullptr)
            return false;

        if (slot < 0 || slot >= SQL_MAX_NUM)
            return false;

        metin2::log::debug("Opening direct sql connection");
        direct_sql_[slot] = std::make_unique<async_sql>();
        if (!direct_sql_[slot]->setup(db_address, user, pwd, db_name, CONNECTION_CHARSET, true, db_port)) {
            clear();
            return false;
        }

        metin2::log::debug("Opening main sql connection");
        main_sql_[slot] = std::make_unique<async_sql>();
        if (!main_sql_[slot]->setup(db_address, user, pwd, db_name, CONNECTION_CHARSET, false, db_port)) {
            clear();
            return false;
        }

        metin2::log::debug("Opening async sql connection");
        async_sql_[slot] = std::make_unique<async_sql>();
        if (!async_sql_[slot]->setup(db_address, user, pwd, db_name, CONNECTION_CHARSET, false, db_port)) {
            clear();
            return false;
        }

        return true;
    }

    auto db_manager::direct_query(std::string_view query, int slot) -> std::unique_ptr<sql_msg> {
        return direct_sql_[slot]->direct_query(std::string(query).c_str());
    }

    void db_manager::return_query(std::string_view query, sql_callback on_result, int slot) {
        assert(slot < SQL_MAX_NUM);
        main_sql_[slot]->return_query(std::string(query).c_str(), std::move(on_result));
        ++query_count[0];
    }

    void db_manager::async_query(std::string_view query, int slot) {
        assert(slot < SQL_MAX_NUM);
        async_sql_[slot]->async_query(std::string(query).c_str());
        ++query_count[1];
    }
}
