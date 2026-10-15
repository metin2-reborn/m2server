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

// One result set of a query (a multi-statement query has several).
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

// A query and, once it ran, its results. Returned to callers by DirectQuery and PopResult;
// they delete it.
struct ql_msg {
    // Collects every result set of the last query on sql.
    void store(MYSQL *sql);

    // The first result set, or nullptr when the query produced none.
    auto get() -> ql_result * {
        return vec_pk_result.empty() ? nullptr : vec_pk_result.front().get();
    }

    int id{0};
    std::string query;
    std::vector<std::unique_ptr<ql_result>> vec_pk_result;
    void *pv_user_data{nullptr};
    bool return_{false}; // the caller collects the result with PopResult
    unsigned int sql_errno{0};
};

// A MySQL connection. With a worker thread (the default) AsyncQuery/ReturnQuery queue queries
// that run in order on that thread; ReturnQuery results come back through PopResult. Without
// one (Setup with bNoThread) only DirectQuery is used, on the caller's thread.
class async_sql {
  public:
    async_sql() = default;
    async_sql(const async_sql &) = delete;
    auto operator=(const async_sql &) -> async_sql & = delete;
    ~async_sql();

    auto setup(const char *c_pszHost, const char *c_pszUser, const char *c_pszPassword, const char *c_pszDB, const char *c_pszLocale, bool bNoThread = false, int iPort = 0) -> bool;
    void quit();

    // Applies m_stLocale as the connection character set.
    auto query_locale_set() -> bool;
    void set_locale(const std::string &stLocale);

    void async_query(const char *c_pszQuery);
    void return_query(const char *c_pszQuery, void *pvUserData);
    auto direct_query(const char *c_pszQuery) -> ql_msg *;
    auto pop_result(ql_msg **pp) -> bool;

    auto count_query() -> uint32_t;
    auto count_result() -> uint32_t;
    auto count_query_finished() -> int;
    void reset_query_finished();
    auto copied_query_count() -> int;
    void reset_copied_query_count();

    auto sql_handle() -> MYSQL *;
    auto escape_string(char *dst, size_t dstSize, const char *src, size_t srcSize) -> size_t;

  private:
    auto connect() -> bool;
    void child_loop();
    auto new_message(const char *c_pszQuery) -> std::unique_ptr<ql_msg>;
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

// Used unqualified throughout game and db.
namespace metin2 {
    using sql::async_sql;
    using sql::ql_msg;
    using sql::ql_result;
}
