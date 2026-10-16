#pragma once

#include <metin2/common/singleton.h>
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

    class db_manager : public singleton<db_manager> {
        std::unique_ptr<async_sql> main_sql_[SQL_MAX_NUM];
        std::unique_ptr<async_sql> direct_sql_[SQL_MAX_NUM];
        std::unique_ptr<async_sql> async_sql_[SQL_MAX_NUM];

      protected:
        void initialize();
        void destroy();

      public:
        db_manager();

        ~db_manager() override;

        void clear();
        void quit();

        auto connect(int slot, const char *db_address, int db_port, const char *db_name, const char *user, const char *pwd) -> int;

        void return_query(std::string_view query, sql_callback on_result, int slot = SQL_PLAYER);
        void async_query(std::string_view query, int slot = SQL_PLAYER);
        auto direct_query(std::string_view query, int slot = SQL_PLAYER) -> std::unique_ptr<sql_msg>;

        auto pop_result() -> std::unique_ptr<sql_msg>;


        auto count_return_query(int i) -> uint32_t { return main_sql_[i] ? main_sql_[i]->count_query() : 0; }
        auto count_return_result(int i) -> uint32_t { return main_sql_[i] ? main_sql_[i]->count_result() : 0; }
        auto count_return_query_finished(int i) -> uint32_t { return main_sql_[i] ? main_sql_[i]->count_query_finished() : 0; }
        auto count_return_copied_query(int i) -> uint32_t { return main_sql_[i] ? main_sql_[i]->copied_query_count() : 0; }

        auto count_async_query(int i) -> uint32_t { return async_sql_[i] ? async_sql_[i]->count_query() : 0; }
        auto count_async_result(int i) -> uint32_t { return async_sql_[i] ? async_sql_[i]->count_result() : 0; }
        auto count_async_query_finished(int i) -> uint32_t { return async_sql_[i] ? async_sql_[i]->count_query_finished() : 0; }
        auto count_async_copied_query(int i) -> uint32_t { return async_sql_[i] ? async_sql_[i]->copied_query_count() : 0; }

        void reset_counter() {
            for (int i = 0; i < SQL_MAX_NUM; ++i) {
                if (main_sql_[i]) {
                    main_sql_[i]->reset_query_finished();
                    main_sql_[i]->reset_copied_query_count();
                }

                if (async_sql_[i]) {
                    async_sql_[i]->reset_query_finished();
                    async_sql_[i]->reset_copied_query_count();
                }
            }
        }
    };
}
