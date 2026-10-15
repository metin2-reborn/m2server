#include <metin2/sql/async_sql.h>

#include "mariadb_com.h"

#include <metin2/log/log.h>

#include <mysql.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <iterator>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include <errmsg.h>
#include <mysqld_error.h>

namespace metin2::sql {

namespace {
    // Errors after which the same query is tried again: the connection or the server is in
    // trouble, not the query.
    auto is_transient_error(unsigned int error) -> bool {
        switch (error) {
        case CR_SOCKET_CREATE_ERROR:
        case CR_CONNECTION_ERROR:
        case CR_IPSOCK_ERROR:
        case CR_UNKNOWN_HOST:
        case CR_SERVER_GONE_ERROR:
        case CR_CONN_HOST_ERROR:
        case ER_NOT_KEYFILE:
        case ER_CRASHED_ON_USAGE:
        case ER_CANT_OPEN_FILE:
        case ER_HOST_NOT_PRIVILEGED:
        case ER_HOST_IS_BLOCKED:
        case ER_PASSWORD_NOT_ALLOWED:
        case ER_PASSWORD_NO_MATCH:
        case ER_CANT_CREATE_THREAD:
        case ER_INVALID_USE_OF_NULL:  return true;
        default:                      return false;
        }
    }

    constexpr auto LONG_QUERY = std::chrono::milliseconds(500);
    constexpr auto RETRY_DELAY = std::chrono::seconds(1);
}

void ql_msg::store(MYSQL *sql) {
    do {
        auto result = std::make_unique<ql_result>();

        result->sql_result = mysql_store_result(sql);
        result->insert_id = mysql_insert_id(sql);
        result->affected_rows = mysql_affected_rows(sql);
        result->num_rows = result->sql_result ? mysql_num_rows(result->sql_result) : 0;

        vec_pk_result.push_back(std::move(result));
    } while (!mysql_next_result(sql));
}

async_sql::~async_sql() {
    quit();

    if (initialized_) {
        metin2::log::info( "AsyncSQL: closing mysql connection.");
        mysql_close(&db_);
    }
}

auto async_sql::setup(const char *c_pszHost, const char *c_pszUser, const char *c_pszPassword, const char *c_pszDB, const char *c_pszLocale, bool bNoThread, int iPort) -> bool {
    host_ = c_pszHost;
    user_ = c_pszUser;
    password_ = c_pszPassword;
    m_st_db_ = c_pszDB;
    port_ = iPort;

    if (c_pszLocale) {
        locale_ = c_pszLocale;
        metin2::log::info( "AsyncSQL: locale {}", locale_.c_str());
    }

    if (bNoThread) {
        return connect();
    }

    thread_ = std::thread([this]() -> void {
        if (connect()) {
            child_loop();
        }
    });
    return true;
}

void async_sql::quit() {
    {
        std::lock_guard lock(mtx_query_);
        end_ = true;
    }
    cv_query_.notify_all();

    if (thread_.joinable()) {
        thread_.join();
    }
}

auto async_sql::connect() -> bool {
    if (!mysql_init(&db_)) {
        metin2::log::error("mysql_init failed");
        return false;
    }
    initialized_ = true;

    if (!locale_.empty() && mysql_options(&db_, MYSQL_SET_CHARSET_NAME, locale_.c_str()) != 0) {
        metin2::log::error("mysql_options MYSQL_SET_CHARSET_NAME {}: {}", locale_.c_str(), mysql_error(&db_));
    }

    if (!mysql_real_connect(&db_, host_.c_str(), user_.c_str(), password_.c_str(), m_st_db_.c_str(), port_, nullptr, CLIENT_MULTI_STATEMENTS)) {
        metin2::log::error("mysql_real_connect: {}", mysql_error(&db_));
        return false;
    }

    my_bool reconnect = true;
    if (mysql_options(&db_, MYSQL_OPT_RECONNECT, &reconnect) != 0) {
        metin2::log::error("mysql_options MYSQL_OPT_RECONNECT: {}", mysql_error(&db_));
    }

    metin2::log::info( "AsyncSQL: connected to {} (reconnect {:d})", host_.c_str(), reconnect);

    thread_id_ = mysql_thread_id(&db_);
    return true;
}

auto async_sql::query_locale_set() -> bool {
    if (locale_.empty() || locale_ == "ascii") {
        metin2::log::error("no connection character set to apply (locale \"{}\")", locale_.c_str());
        return true;
    }

    if (mysql_set_character_set(&db_, locale_.c_str())) {
        metin2::log::error("cannot set locale {} by 'mysql_set_character_set', errno {:d} {}", locale_.c_str(), mysql_errno(&db_), mysql_error(&db_));
        return false;
    }

    metin2::log::info( "\t--mysql_set_character_set({})", locale_.c_str());
    return true;
}

void async_sql::set_locale(const std::string &stLocale) {
    locale_ = stLocale;
    query_locale_set();
}

// After an automatic reconnect the session lost its character set.
void async_sql::check_reconnected() {
    if (thread_id_ == mysql_thread_id(&db_)) {
        return;
    }

    metin2::log::error("MySQL connection was reconnected. querying locale set");
    while (!query_locale_set()) {
        std::this_thread::sleep_for(RETRY_DELAY);
    }
    thread_id_ = mysql_thread_id(&db_);
}

auto async_sql::new_message(const char *c_pszQuery) -> std::unique_ptr<ql_msg> {
    auto msg = std::make_unique<ql_msg>();
    msg->id = ++msg_count_;
    msg->query = c_pszQuery;
    return msg;
}

auto async_sql::direct_query(const char *c_pszQuery) -> ql_msg * {
    check_reconnected();

    auto msg = new_message(c_pszQuery);

    if (mysql_real_query(&db_, msg->query.c_str(), msg->query.length())) {
        metin2::log::error("AsyncSQL::DirectQuery : mysql_query error: {}\nquery: {}", mysql_error(&db_), msg->query.c_str());
        msg->sql_errno = mysql_errno(&db_);
    }

    msg->store(&db_);
    return msg.release();
}

void async_sql::async_query(const char *c_pszQuery) {
    push_query(new_message(c_pszQuery));
}

void async_sql::return_query(const char *c_pszQuery, void *pvUserData) {
    auto msg = new_message(c_pszQuery);
    msg->returns_result = true;
    msg->pv_user_data = pvUserData;
    push_query(std::move(msg));
}

void async_sql::push_query(std::unique_ptr<ql_msg> msg) {
    {
        std::lock_guard lock(mtx_query_);
        queue_query_.push_back(std::move(msg));
    }
    cv_query_.notify_one();
}

auto async_sql::pop_result(ql_msg **pp) -> bool {
    std::lock_guard lock(mtx_result_);

    if (queue_result_.empty()) {
        return false;
    }

    *pp = queue_result_.front().release();
    queue_result_.pop_front();
    return true;
}

// Runs the query; false when it failed with a transient error and should be tried again.
auto async_sql::execute(ql_msg &msg) -> bool {
    check_reconnected();

    const auto start = std::chrono::steady_clock::now();

    if (mysql_real_query(&db_, msg.query.c_str(), msg.query.length())) {
        msg.sql_errno = mysql_errno(&db_);
        metin2::log::error("AsyncSQL: query failed: {} (query: {} errno: {:d})", mysql_error(&db_), msg.query.c_str(), msg.sql_errno);

        if (is_transient_error(msg.sql_errno)) {
            return false;
        }
    }

    const auto elapsed = std::chrono::steady_clock::now() - start;
    if (elapsed > LONG_QUERY) {
        metin2::log::info( "[QUERY : LONG INTERVAL(OverSec {:.3f})] : {}", std::chrono::duration<double>(elapsed).count(), msg.query.c_str());
    }

    return true;
}

// A finished query: results go to the result queue when the caller wants them.
void async_sql::finish(std::unique_ptr<ql_msg> msg) {
    if (msg->returns_result) {
        msg->store(&db_);
        std::lock_guard lock(mtx_result_);
        queue_result_.push_back(std::move(msg));
    }

    ++query_finished_;
}

void async_sql::child_loop() {
    std::deque<std::unique_ptr<ql_msg>> batch;

    while (true) {
        {
            std::unique_lock lock(mtx_query_);
            cv_query_.wait(lock, [this]() -> bool { return end_ || !queue_query_.empty(); });

            if (queue_query_.empty()) {
                break; // m_bEnd and nothing left to run
            }

            copied_query_ += static_cast<int>(queue_query_.size());
            std::ranges::move(queue_query_, std::back_inserter(batch));
            queue_query_.clear();
        }

        while (!batch.empty()) {
            if (!execute(*batch.front())) {
                if (end_) {
                    // shutting down: the server is unreachable, give up on this query
                    metin2::log::error("AsyncSQL: dropping query at shutdown: {}", batch.front()->query.c_str());
                } else {
                    metin2::log::error("AsyncSQL: retrying");
                    std::this_thread::sleep_for(RETRY_DELAY);
                    continue;
                }
            }

            if (end_) {
                metin2::log::info( "QUERY_FLUSH: {}", batch.front()->query.c_str());
            }

            finish(std::move(batch.front()));
            batch.pop_front();
        }
    }
}

auto async_sql::count_query() -> uint32_t {
    std::lock_guard lock(mtx_query_);
    return queue_query_.size();
}

auto async_sql::count_result() -> uint32_t {
    std::lock_guard lock(mtx_result_);
    return queue_result_.size();
}

auto async_sql::count_query_finished() -> int {
    return query_finished_;
}

void async_sql::reset_query_finished() {
    query_finished_ = 0;
}

auto async_sql::copied_query_count() -> int {
    return copied_query_;
}

void async_sql::reset_copied_query_count() {
    copied_query_ = 0;
}

auto async_sql::sql_handle() -> MYSQL * {
    return &db_;
}

auto async_sql::escape_string(char *dst, size_t dstSize, const char *src, size_t srcSize) -> size_t {
    if (srcSize == 0) {
        std::ranges::fill_n(dst, static_cast<std::ptrdiff_t>(dstSize), '\0');
        return 0;
    }

    if (dstSize == 0) {
        return 0;
    }

    if (dstSize < srcSize * 2 + 1) {
        // src may not be terminated: log at most 255 bytes of it
        const std::string_view shown(src, std::min<size_t>(srcSize, 255));

        metin2::log::error("FATAL ERROR!! not enough buffer size (dstSize {:d} srcSize {:d} src{}: {})",
            dstSize, srcSize, shown.size() != srcSize ? "(trimmed to 255 characters)" : "", std::string_view(shown.data(), static_cast<int>(shown.size())));

        dst[0] = '\0';
        return 0;
    }

    return mysql_real_escape_string(&db_, dst, src, srcSize);
}
}
