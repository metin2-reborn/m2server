#pragma once

#include <metin2/sql/async_sql.h>

#include <cstdint>
#include <memory>
#include <string_view>

namespace metin2::db::inline storage {

    enum e_sql_slot {
        SQL_PLAYER,
        SQL_ACCOUNT,
        SQL_COMMON,
        SQL_MAX_NUM,
    };

    class db_manager {
        std::unique_ptr<async_sql> main_sql_[SQL_MAX_NUM];
        std::unique_ptr<async_sql> direct_sql_[SQL_MAX_NUM];
        std::unique_ptr<async_sql> async_sql_[SQL_MAX_NUM];

      public:
        db_manager() = default;

        ~db_manager();

        void clear();
        void quit();

        auto connect(int slot, const char *db_address, int db_port, const char *db_name, const char *user, const char *pwd) -> int;

        void return_query(std::string_view query, sql_callback on_result, int slot = SQL_PLAYER);
        void async_query(std::string_view query, int slot = SQL_PLAYER);
        auto direct_query(std::string_view query, int slot = SQL_PLAYER) -> std::unique_ptr<sql_msg>;

        auto pop_result() -> std::unique_ptr<sql_msg>;

        auto return_stats(int i) -> sql_stats { return main_sql_[i] ? main_sql_[i]->stats() : sql_stats{}; }
        auto async_stats(int i) -> sql_stats { return async_sql_[i] ? async_sql_[i]->stats() : sql_stats{}; }

        void reset_counter() {
            for (int i = 0; i < SQL_MAX_NUM; ++i) {
                if (main_sql_[i]) {
                    main_sql_[i]->reset_stats();
                }

                if (async_sql_[i]) {
                    async_sql_[i]->reset_stats();
                }
            }
        }
    };

    inline auto get_db_manager() -> db_manager & {
        static db_manager instance;
        return instance;
    }
}
