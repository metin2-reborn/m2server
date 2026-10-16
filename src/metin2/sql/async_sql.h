#pragma once

#include <mysql.h>

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace metin2::sql {
    struct mysql_res_deleter {
        void operator()(MYSQL_RES *res) const { mysql_free_result(res); }
    };

    struct sql_result {
        std::unique_ptr<MYSQL_RES, mysql_res_deleter> res;
        uint32_t num_rows{0};
        uint32_t affected_rows{0};
        uint32_t insert_id{0};
    };

    struct sql_msg;
    using sql_callback = std::move_only_function<void(sql_msg &)>;

    struct sql_msg {
        void store(MYSQL *sql);

        auto get() -> sql_result * {
            return results.empty() ? nullptr : results.front().get();
        }

        int id{0};
        std::string query;
        std::vector<std::unique_ptr<sql_result>> results;
        sql_callback on_result;
        unsigned int sql_errno{0};
    };

    struct sql_stats {
        std::size_t queued{0};
        std::size_t results{0};
        int finished{0};
        int copied{0};
    };

    class async_sql {
      public:
        async_sql() = default;
        async_sql(const async_sql &) = delete;

        auto operator=(const async_sql &) -> async_sql & = delete;

        ~async_sql();

        auto setup(std::string_view host, std::string_view user, std::string_view password, std::string_view db, std::string_view locale, bool no_thread = false, int port = 0) -> bool;
        void quit();

        auto query_locale_set() -> bool;
        void set_locale(const std::string &locale);

        void async_query(const char *query);
        void return_query(const char *query, sql_callback on_result);
        auto direct_query(const char *query_value) -> std::unique_ptr<sql_msg>;
        auto pop_result() -> std::unique_ptr<sql_msg>;

        auto stats() -> sql_stats;
        void reset_stats();

      private:
        auto connect() -> bool;
        void child_loop(std::stop_token stop);
        auto new_message(const char *query_value) -> std::unique_ptr<sql_msg>;
        void push_query(std::unique_ptr<sql_msg> msg);
        void check_reconnected();
        auto execute(sql_msg &msg) -> bool;
        void finish(std::unique_ptr<sql_msg> msg);

        MYSQL db_{};
        bool initialized_{false};

        std::string host_;
        std::string user_;
        std::string password_;
        std::string db_name_;
        std::string locale_;
        int port_{0};

        std::atomic<int> msg_count_{0};
        unsigned long thread_id_{0};

        std::mutex mtx_query_;
        std::condition_variable_any cv_query_;
        std::deque<std::unique_ptr<sql_msg>> queue_query_;

        std::mutex mtx_result_;
        std::deque<std::unique_ptr<sql_msg>> queue_result_;

        std::atomic<int> query_finished_{0};
        std::atomic<int> copied_query_{0};

        std::jthread thread_;
    };
}

namespace metin2 {
    using sql::async_sql;
    using sql::sql_callback;
    using sql::sql_msg;
    using sql::sql_result;
    using sql::sql_stats;
}
