#pragma once

#include <mysql.h>

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace metin2::sql {

struct ql_result {
    ql_result() = default;
    ql_result(const ql_result &) = delete;
    auto operator=(const ql_result &) -> ql_result & = delete;

    ~ql_result() {
        if (sql_result) {
            mysql_free_result(sql_result);
        }
    }

    MYSQL_RES *sql_result{nullptr};
    uint32_t num_rows{0};
    uint32_t affected_rows{0};
    uint32_t insert_id{0};
};

struct ql_msg {
    void store(MYSQL *sql);

    auto get() -> ql_result * {
        return vec_pk_result.empty() ? nullptr : vec_pk_result.front().get();
    }

    int id{0};
    std::string query;
    std::vector<std::unique_ptr<ql_result>> vec_pk_result;
    void *pv_user_data{nullptr};
    bool returns_result{false};
    unsigned int sql_errno{0};
};

class async_sql {
  public:
    async_sql() = default;
    async_sql(const async_sql &) = delete;
    auto operator=(const async_sql &) -> async_sql & = delete;
    ~async_sql();

    auto setup(const char *host, const char *user, const char *password, const char *db, const char *locale, bool no_thread = false, int port = 0) -> bool;
    void quit();

    auto query_locale_set() -> bool;
    void set_locale(const std::string &locale);

    void async_query(const char *query);
    void return_query(const char *query, void *user_data);
    auto direct_query(const char *query_value) -> ql_msg *;
    auto pop_result(ql_msg **pp) -> bool;

    auto count_query() -> uint32_t;
    auto count_result() -> uint32_t;
    auto count_query_finished() -> int;
    void reset_query_finished();
    auto copied_query_count() -> int;
    void reset_copied_query_count();

    auto sql_handle() -> MYSQL *;
    auto escape_string(char *dst, size_t dst_size, const char *src, size_t src_size) -> size_t;

  private:
    auto connect() -> bool;
    void child_loop();
    auto new_message(const char *query_value) -> std::unique_ptr<ql_msg>;
    void push_query(std::unique_ptr<ql_msg> msg);
    void check_reconnected();
    auto execute(ql_msg &msg) -> bool;
    void finish(std::unique_ptr<ql_msg> msg);

    MYSQL db_{};
    bool initialized_{false};

    std::string host_;
    std::string user_;
    std::string password_;
    std::string m_st_db_;
    std::string locale_;
    int port_{0};

    std::atomic<int> msg_count_{0};
    unsigned long thread_id_{0};

    std::mutex mtx_query_;
    std::condition_variable cv_query_;
    std::deque<std::unique_ptr<ql_msg>> queue_query_;

    std::mutex mtx_result_;
    std::deque<std::unique_ptr<ql_msg>> queue_result_;

    std::thread thread_;
    std::atomic<bool> end_{false};

    std::atomic<int> query_finished_{0};
    std::atomic<int> copied_query_{0};
};
}

namespace metin2 {
    using sql::async_sql;
    using sql::ql_msg;
    using sql::ql_result;
}
