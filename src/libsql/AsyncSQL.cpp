#include "AsyncSQL.h"

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
    auto IsTransientError(unsigned int error) -> bool {
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

void SQLMsg::Store(MYSQL *sql) {
    do {
        auto result = std::make_unique<SQLResult>();

        result->pSQLResult = mysql_store_result(sql);
        result->uiInsertID = mysql_insert_id(sql);
        result->uiAffectedRows = mysql_affected_rows(sql);
        result->uiNumRows = result->pSQLResult ? mysql_num_rows(result->pSQLResult) : 0;

        vec_pkResult.push_back(std::move(result));
    } while (!mysql_next_result(sql));
}

CAsyncSQL::~CAsyncSQL() {
    Quit();

    if (m_bInitialized) {
        metin2::log::info( "AsyncSQL: closing mysql connection.");
        mysql_close(&m_hDB);
    }
}

auto CAsyncSQL::Setup(const char *c_pszHost, const char *c_pszUser, const char *c_pszPassword, const char *c_pszDB, const char *c_pszLocale, bool bNoThread, int iPort) -> bool {
    m_stHost = c_pszHost;
    m_stUser = c_pszUser;
    m_stPassword = c_pszPassword;
    m_stDB = c_pszDB;
    m_iPort = iPort;

    if (c_pszLocale) {
        m_stLocale = c_pszLocale;
        metin2::log::info( "AsyncSQL: locale {}", m_stLocale.c_str());
    }

    if (bNoThread) {
        return Connect();
    }

    m_thread = std::thread([this]() -> void {
        if (Connect()) {
            ChildLoop();
        }
    });
    return true;
}

void CAsyncSQL::Quit() {
    {
        std::lock_guard lock(m_mtxQuery);
        m_bEnd = true;
    }
    m_cvQuery.notify_all();

    if (m_thread.joinable()) {
        m_thread.join();
    }
}

auto CAsyncSQL::Connect() -> bool {
    if (!mysql_init(&m_hDB)) {
        metin2::log::error("mysql_init failed");
        return false;
    }
    m_bInitialized = true;

    if (!m_stLocale.empty() && mysql_options(&m_hDB, MYSQL_SET_CHARSET_NAME, m_stLocale.c_str()) != 0) {
        metin2::log::error("mysql_options MYSQL_SET_CHARSET_NAME {}: {}", m_stLocale.c_str(), mysql_error(&m_hDB));
    }

    if (!mysql_real_connect(&m_hDB, m_stHost.c_str(), m_stUser.c_str(), m_stPassword.c_str(), m_stDB.c_str(), m_iPort, nullptr, CLIENT_MULTI_STATEMENTS)) {
        metin2::log::error("mysql_real_connect: {}", mysql_error(&m_hDB));
        return false;
    }

    my_bool reconnect = true;
    if (mysql_options(&m_hDB, MYSQL_OPT_RECONNECT, &reconnect) != 0) {
        metin2::log::error("mysql_options MYSQL_OPT_RECONNECT: {}", mysql_error(&m_hDB));
    }

    metin2::log::info( "AsyncSQL: connected to {} (reconnect {:d})", m_stHost.c_str(), reconnect);

    m_ulThreadID = mysql_thread_id(&m_hDB);
    return true;
}

auto CAsyncSQL::QueryLocaleSet() -> bool {
    if (m_stLocale.empty() || m_stLocale == "ascii") {
        metin2::log::error("no connection character set to apply (locale \"{}\")", m_stLocale.c_str());
        return true;
    }

    if (mysql_set_character_set(&m_hDB, m_stLocale.c_str())) {
        metin2::log::error("cannot set locale {} by 'mysql_set_character_set', errno {:d} {}", m_stLocale.c_str(), mysql_errno(&m_hDB), mysql_error(&m_hDB));
        return false;
    }

    metin2::log::info( "\t--mysql_set_character_set({})", m_stLocale.c_str());
    return true;
}

void CAsyncSQL::SetLocale(const std::string &stLocale) {
    m_stLocale = stLocale;
    QueryLocaleSet();
}

// After an automatic reconnect the session lost its character set.
void CAsyncSQL::CheckReconnected() {
    if (m_ulThreadID == mysql_thread_id(&m_hDB)) {
        return;
    }

    metin2::log::error("MySQL connection was reconnected. querying locale set");
    while (!QueryLocaleSet()) {
        std::this_thread::sleep_for(RETRY_DELAY);
    }
    m_ulThreadID = mysql_thread_id(&m_hDB);
}

auto CAsyncSQL::NewMessage(const char *c_pszQuery) -> std::unique_ptr<SQLMsg> {
    auto msg = std::make_unique<SQLMsg>();
    msg->iID = ++m_iMsgCount;
    msg->stQuery = c_pszQuery;
    return msg;
}

auto CAsyncSQL::DirectQuery(const char *c_pszQuery) -> SQLMsg * {
    CheckReconnected();

    auto msg = NewMessage(c_pszQuery);

    if (mysql_real_query(&m_hDB, msg->stQuery.c_str(), msg->stQuery.length())) {
        metin2::log::error("AsyncSQL::DirectQuery : mysql_query error: {}\nquery: {}", mysql_error(&m_hDB), msg->stQuery.c_str());
        msg->uiSQLErrno = mysql_errno(&m_hDB);
    }

    msg->Store(&m_hDB);
    return msg.release();
}

void CAsyncSQL::AsyncQuery(const char *c_pszQuery) {
    PushQuery(NewMessage(c_pszQuery));
}

void CAsyncSQL::ReturnQuery(const char *c_pszQuery, void *pvUserData) {
    auto msg = NewMessage(c_pszQuery);
    msg->bReturn = true;
    msg->pvUserData = pvUserData;
    PushQuery(std::move(msg));
}

void CAsyncSQL::PushQuery(std::unique_ptr<SQLMsg> msg) {
    {
        std::lock_guard lock(m_mtxQuery);
        m_queue_query.push_back(std::move(msg));
    }
    m_cvQuery.notify_one();
}

auto CAsyncSQL::PopResult(SQLMsg **pp) -> bool {
    std::lock_guard lock(m_mtxResult);

    if (m_queue_result.empty()) {
        return false;
    }

    *pp = m_queue_result.front().release();
    m_queue_result.pop_front();
    return true;
}

// Runs the query; false when it failed with a transient error and should be tried again.
auto CAsyncSQL::Execute(SQLMsg &msg) -> bool {
    CheckReconnected();

    const auto start = std::chrono::steady_clock::now();

    if (mysql_real_query(&m_hDB, msg.stQuery.c_str(), msg.stQuery.length())) {
        msg.uiSQLErrno = mysql_errno(&m_hDB);
        metin2::log::error("AsyncSQL: query failed: {} (query: {} errno: {:d})", mysql_error(&m_hDB), msg.stQuery.c_str(), msg.uiSQLErrno);

        if (IsTransientError(msg.uiSQLErrno)) {
            return false;
        }
    }

    const auto elapsed = std::chrono::steady_clock::now() - start;
    if (elapsed > LONG_QUERY) {
        metin2::log::info( "[QUERY : LONG INTERVAL(OverSec {:.3f})] : {}", std::chrono::duration<double>(elapsed).count(), msg.stQuery.c_str());
    }

    return true;
}

// A finished query: results go to the result queue when the caller wants them.
void CAsyncSQL::Finish(std::unique_ptr<SQLMsg> msg) {
    if (msg->bReturn) {
        msg->Store(&m_hDB);
        std::lock_guard lock(m_mtxResult);
        m_queue_result.push_back(std::move(msg));
    }

    ++m_iQueryFinished;
}

void CAsyncSQL::ChildLoop() {
    std::deque<std::unique_ptr<SQLMsg>> batch;

    while (true) {
        {
            std::unique_lock lock(m_mtxQuery);
            m_cvQuery.wait(lock, [this]() -> bool { return m_bEnd || !m_queue_query.empty(); });

            if (m_queue_query.empty()) {
                break; // m_bEnd and nothing left to run
            }

            m_iCopiedQuery += static_cast<int>(m_queue_query.size());
            std::ranges::move(m_queue_query, std::back_inserter(batch));
            m_queue_query.clear();
        }

        while (!batch.empty()) {
            if (!Execute(*batch.front())) {
                if (m_bEnd) {
                    // shutting down: the server is unreachable, give up on this query
                    metin2::log::error("AsyncSQL: dropping query at shutdown: {}", batch.front()->stQuery.c_str());
                } else {
                    metin2::log::error("AsyncSQL: retrying");
                    std::this_thread::sleep_for(RETRY_DELAY);
                    continue;
                }
            }

            if (m_bEnd) {
                metin2::log::info( "QUERY_FLUSH: {}", batch.front()->stQuery.c_str());
            }

            Finish(std::move(batch.front()));
            batch.pop_front();
        }
    }
}

auto CAsyncSQL::CountQuery() -> uint32_t {
    std::lock_guard lock(m_mtxQuery);
    return m_queue_query.size();
}

auto CAsyncSQL::CountResult() -> uint32_t {
    std::lock_guard lock(m_mtxResult);
    return m_queue_result.size();
}

auto CAsyncSQL::CountQueryFinished() -> int {
    return m_iQueryFinished;
}

void CAsyncSQL::ResetQueryFinished() {
    m_iQueryFinished = 0;
}

auto CAsyncSQL::GetCopiedQueryCount() -> int {
    return m_iCopiedQuery;
}

void CAsyncSQL::ResetCopiedQueryCount() {
    m_iCopiedQuery = 0;
}

auto CAsyncSQL::GetSQLHandle() -> MYSQL * {
    return &m_hDB;
}

auto CAsyncSQL::EscapeString(char *dst, size_t dstSize, const char *src, size_t srcSize) -> size_t {
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

    return mysql_real_escape_string(&m_hDB, dst, src, srcSize);
}
}
