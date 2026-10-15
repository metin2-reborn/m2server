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

struct sql_result {
    sql_result() = default;
    sql_result(const sql_result &) = delete;
    auto operator=(const sql_result &) -> sql_result & = delete;

    ~sql_result() {
        if (res) {
            mysql_free_result(res);
        }
    }

    MYSQL_RES *res{nullptr};
    uint32_t num_rows{0};
    uint32_t affected_rows{0};
    uint32_t insert_id{0};
};

struct sql_msg {
    void store(MYSQL *sql);

    auto get() -> sql_result * {
        return results.empty() ? nullptr : results.front().get();
    }

    int id{0};
    std::string query;
    std::vector<std::unique_ptr<sql_result>> results;
    void *user_data{nullptr};
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
    auto direct_query(const char *query_value) -> sql_msg *;
    auto pop_result(sql_msg **pp) -> bool;

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
    std::string m_st_db_;
    std::string locale_;
    int port_{0};

    std::atomic<int> msg_count_{0};
    unsigned long thread_id_{0};

    std::mutex mtx_query_;
    std::condition_variable cv_query_;
    std::deque<std::unique_ptr<sql_msg>> queue_query_;

    std::mutex mtx_result_;
    std::deque<std::unique_ptr<sql_msg>> queue_result_;

    std::thread thread_;
    std::atomic<bool> end_{false};

    std::atomic<int> query_finished_{0};
    std::atomic<int> copied_query_{0};
};
}

namespace metin2 {
    using sql::async_sql;
    using sql::sql_msg;
    using sql::sql_result;
}
