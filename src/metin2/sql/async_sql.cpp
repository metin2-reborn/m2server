#include <metin2/sql/async_sql.h>

#include "mariadb_com.h"

#include <metin2/log/log.h>

#include <mysql.h>

#include <algorithm>
#include <chrono>
#include <deque>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include <errmsg.h>
#include <mysqld_error.h>

namespace metin2::sql {
    namespace {
        // The server is unreachable, or the statement may simply be run again. Any other error is a problem with the
        // query itself; retrying that would hold up every query queued behind it. After CR_SERVER_LOST the statement
        // may already have run: saves are UPDATE/REPLACE and repeat harmlessly, a log INSERT may be doubled.
        auto is_transient_error(unsigned int error) -> bool {
            switch (error) {
            case CR_SOCKET_CREATE_ERROR:
            case CR_CONNECTION_ERROR:
            case CR_IPSOCK_ERROR:
            case CR_UNKNOWN_HOST:
            case CR_SERVER_GONE_ERROR:
            case CR_SERVER_LOST:
            case CR_CONN_HOST_ERROR:
            case ER_CON_COUNT_ERROR:
            case ER_SERVER_SHUTDOWN:
            case ER_ACCESS_DENIED_ERROR:
            case ER_HOST_NOT_PRIVILEGED:
            case ER_HOST_IS_BLOCKED:
            case ER_CANT_CREATE_THREAD:
            case ER_LOCK_WAIT_TIMEOUT:
            case ER_LOCK_DEADLOCK:       return true;
            default:                     return false;
            }
        }

        constexpr auto LONG_QUERY = std::chrono::milliseconds(500);
        constexpr auto RETRY_DELAY = std::chrono::seconds(1);
        constexpr auto MAX_RETRY_DELAY = std::chrono::seconds(10);
        constexpr auto CONNECT_TIMEOUT = std::chrono::seconds(5);
        constexpr auto IO_TIMEOUT = std::chrono::seconds(30);
        // Within the 120 s compose stop grace period, so queued saves get a chance once MariaDB is back.
        constexpr auto SHUTDOWN_RETRY_LIMIT = std::chrono::seconds(60);
        // Direct queries block the main loop, so they only get a short retry.
        constexpr int DIRECT_QUERY_ATTEMPTS = 5;

        // One window for the whole process: the connections are stopped one after another, and all of them have to
        // be done within the stop grace period. The first connection that fails at shutdown starts it.
        auto shutdown_deadline() -> std::chrono::steady_clock::time_point {
            static const auto deadline = std::chrono::steady_clock::now() + SHUTDOWN_RETRY_LIMIT;
            return deadline;
        }
    }

    void sql_msg::store(MYSQL *sql) {
        do {
            auto result = std::make_unique<sql_result>();

            result->res.reset(mysql_store_result(sql));
            result->insert_id = mysql_insert_id(sql);
            result->affected_rows = mysql_affected_rows(sql);
            result->num_rows = result->res ? mysql_num_rows(result->res.get()) : 0;

            results.push_back(std::move(result));
        } while (!mysql_next_result(sql));
    }

    async_sql::~async_sql() {
        quit();

        if (initialized_) {
            metin2::log::info("Sql: closing connection");

            mysql_close(&db_);
        }
    }

    auto async_sql::setup(std::string_view host, std::string_view user, std::string_view password, std::string_view db, std::string_view locale, bool no_thread, int port) -> bool {
        host_ = host;
        user_ = user;
        password_ = password;
        db_name_ = db;
        port_ = port;
        locale_ = locale;

        metin2::log::info("Sql: connection character set {}", locale_);

        if (no_thread) {
            return connect();
        }

        thread_ = std::jthread([this](std::stop_token stop) -> void {
            auto delay = RETRY_DELAY;
            while (!connect()) {
                if (stop.stop_requested() && std::chrono::steady_clock::now() >= shutdown_deadline()) {
                    drop_queued("cannot connect at shutdown");
                    return;
                }

                metin2::log::warn("Sql: cannot connect to {}, retrying in {} s", host_, delay.count());
                std::this_thread::sleep_for(delay);
                delay = std::min(delay * 2, MAX_RETRY_DELAY);
            }

            child_loop(std::move(stop));
        });

        return true;
    }

    void async_sql::quit() {
        thread_.request_stop();

        if (thread_.joinable()) {
            thread_.join();
        }
    }

    auto async_sql::connect() -> bool {
        if (initialized_) {
            mysql_close(&db_);
            initialized_ = false;
        }

        if (!mysql_init(&db_)) {
            metin2::log::error("Cannot initialise mysql client handle");
            return false;
        }
        initialized_ = true;

        // Without timeouts a hung server would block a query forever; with them it reads as a lost connection.
        const unsigned int connect_timeout = CONNECT_TIMEOUT.count();
        const unsigned int io_timeout = IO_TIMEOUT.count();
        mysql_options(&db_, MYSQL_OPT_CONNECT_TIMEOUT, &connect_timeout);
        mysql_options(&db_, MYSQL_OPT_READ_TIMEOUT, &io_timeout);
        mysql_options(&db_, MYSQL_OPT_WRITE_TIMEOUT, &io_timeout);

        if (!locale_.empty() && mysql_options(&db_, MYSQL_SET_CHARSET_NAME, locale_.c_str()) != 0) {
            metin2::log::error("Cannot set connection character set {}: {}", locale_, mysql_error(&db_));
        }

        if (!mysql_real_connect(&db_, host_.c_str(), user_.c_str(), password_.c_str(), db_name_.c_str(), port_, nullptr, CLIENT_MULTI_STATEMENTS)) {
            metin2::log::error("Cannot connect to mysql: {}", mysql_error(&db_));
            return false;
        }

        my_bool reconnect = true;
        if (mysql_options(&db_, MYSQL_OPT_RECONNECT, &reconnect) != 0) {
            metin2::log::error("Cannot enable mysql auto-reconnect: {}", mysql_error(&db_));
        }

        metin2::log::info("Sql: connected to {} (auto-reconnect {:d})", host_, reconnect);

        thread_id_ = mysql_thread_id(&db_);
        return true;
    }

    auto async_sql::query_locale_set() -> bool {
        if (locale_.empty() || locale_ == "ascii") {
            metin2::log::warn("No connection character set to apply (locale \"{}\")", locale_);
            return true;
        }

        if (mysql_set_character_set(&db_, locale_.c_str())) {
            metin2::log::error("Cannot set connection character set {}: errno {:d} {}", locale_, mysql_errno(&db_), mysql_error(&db_));
            return false;
        }

        metin2::log::debug("Sql: character set {} applied", locale_);
        return true;
    }

    void async_sql::set_locale(const std::string &locale) {
        locale_ = locale;
        query_locale_set();
    }

    void async_sql::check_reconnected() {
        if (thread_id_ == mysql_thread_id(&db_)) {
            return;
        }

        metin2::log::info("Sql: reconnected to {}, re-applying the character set", host_);
        if (query_locale_set()) {
            thread_id_ = mysql_thread_id(&db_);
        }
    }

    auto async_sql::new_message(const char *query_value) -> std::unique_ptr<sql_msg> {
        auto msg = std::make_unique<sql_msg>();
        msg->id = ++msg_count_;
        msg->query = query_value;
        return msg;
    }

    auto async_sql::direct_query(const char *query_value) -> std::unique_ptr<sql_msg> {
        check_reconnected();

        auto msg = new_message(query_value);

        for (int attempt = 1;; ++attempt) {
            msg->sql_errno = 0;
            if (!mysql_real_query(&db_, msg->query.c_str(), msg->query.length())) {
                break;
            }

            msg->sql_errno = mysql_errno(&db_);
            if (!is_transient_error(msg->sql_errno) || attempt == DIRECT_QUERY_ATTEMPTS) {
                metin2::log::error("Direct query failed: {} (query: {})", mysql_error(&db_), msg->query);
                break;
            }

            metin2::log::warn("Direct query failed: {}; retrying ({}/{})", mysql_error(&db_), attempt, DIRECT_QUERY_ATTEMPTS - 1);
            std::this_thread::sleep_for(RETRY_DELAY);
            check_reconnected();
        }

        msg->store(&db_);
        return msg;
    }

    void async_sql::async_query(const char *query) {
        push_query(new_message(query));
    }

    void async_sql::return_query(const char *query, sql_callback on_result) {
        auto msg = new_message(query);
        msg->on_result = std::move(on_result);
        push_query(std::move(msg));
    }

    void async_sql::push_query(std::unique_ptr<sql_msg> msg) {
        {
            std::lock_guard lock(mtx_query_);
            queries_.push_back(std::move(msg));
        }
        cv_query_.notify_one();
    }

    auto async_sql::pop_result() -> std::unique_ptr<sql_msg> {
        std::lock_guard lock(mtx_result_);

        if (results_.empty()) {
            return nullptr;
        }

        auto msg = std::move(results_.front());
        results_.pop_front();
        return msg;
    }

    auto async_sql::execute(sql_msg &msg) -> bool {
        check_reconnected();

        const auto start = std::chrono::steady_clock::now();

        msg.sql_errno = 0;
        if (mysql_real_query(&db_, msg.query.c_str(), msg.query.length())) {
            msg.sql_errno = mysql_errno(&db_);
            if (is_transient_error(msg.sql_errno)) {
                last_error_ = mysql_error(&db_);
                return false;
            }

            metin2::log::error("Query failed: {} (query: {}, errno {:d})", mysql_error(&db_), msg.query, msg.sql_errno);
        }

        const auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed > LONG_QUERY) {
            metin2::log::warn("Slow query took {:.3f} s: {}", std::chrono::duration<double>(elapsed).count(), msg.query);
        }

        return true;
    }

    void async_sql::finish(std::unique_ptr<sql_msg> msg) {
        if (msg->on_result) {
            msg->store(&db_);
            std::lock_guard lock(mtx_result_);
            results_.push_back(std::move(msg));
        }

        ++query_finished_;
    }

    void async_sql::child_loop(std::stop_token stop) {
        std::deque<std::unique_ptr<sql_msg>> batch;
        int failures = 0;
        auto delay = RETRY_DELAY;
        bool warned_at_shutdown = false;

        while (true) {
            {
                std::unique_lock lock(mtx_query_);
                cv_query_.wait(lock, stop, [this]() -> bool { return !queries_.empty(); });

                if (queries_.empty()) {
                    break;
                }

                copied_query_ += static_cast<int>(queries_.size());
                batch.swap(queries_);
            }

            while (!batch.empty()) {
                if (execute(*batch.front())) {
                    if (failures > 0) {
                        metin2::log::info("Sql: connection to {} is back after {} failed attempts", host_, failures);
                        failures = 0;
                        delay = RETRY_DELAY;
                    }

                    finish(std::move(batch.front()));
                    batch.pop_front();
                    continue;
                }

                if (failures++ == 0) {
                    metin2::log::error("Sql: lost the connection to {} ({}); holding {} queries until it is back", host_, last_error_, batch.size() + stats().queued);
                }

                if (stop.stop_requested()) {
                    const auto deadline = shutdown_deadline();
                    if (!warned_at_shutdown) {
                        warned_at_shutdown = true;
                        const auto left = std::chrono::duration_cast<std::chrono::seconds>(deadline - std::chrono::steady_clock::now());
                        metin2::log::warn("Sql: shutting down while {} is unreachable; retrying for up to {} s", host_, std::max<long long>(0, left.count()));
                    }

                    if (std::chrono::steady_clock::now() >= deadline) {
                        for (const auto &msg : batch) {
                            metin2::log::error("Sql: dropping query at shutdown: {}", msg->query);
                        }
                        batch.clear();
                        drop_queued("unreachable at shutdown");
                        return;
                    }

                    delay = RETRY_DELAY;
                }

                std::this_thread::sleep_for(delay);
                delay = std::min(delay * 2, MAX_RETRY_DELAY);
            }
        }
    }

    void async_sql::drop_queued(std::string_view reason) {
        std::lock_guard lock(mtx_query_);
        for (const auto &msg : queries_) {
            metin2::log::error("Sql: dropping query ({}): {}", reason, msg->query);
        }
        queries_.clear();
    }

    auto async_sql::stats() -> sql_stats {
        sql_stats result{
            .finished = query_finished_,
            .copied = copied_query_,
        };

        {
            std::lock_guard lock(mtx_query_);
            result.queued = queries_.size();
        }

        {
            std::lock_guard lock(mtx_result_);
            result.results = results_.size();
        }

        return result;
    }

    void async_sql::reset_stats() {
        query_finished_ = 0;
        copied_query_ = 0;
    }
}
