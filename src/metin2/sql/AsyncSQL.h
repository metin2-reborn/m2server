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
struct SQLResult {
    SQLResult() = default;
    SQLResult(const SQLResult &) = delete;
    auto operator=(const SQLResult &) -> SQLResult & = delete;

    ~SQLResult() {
        if (pSQLResult) {
            mysql_free_result(pSQLResult);
        }
    }

    MYSQL_RES *pSQLResult{nullptr};
    uint32_t uiNumRows{0};
    uint32_t uiAffectedRows{0};
    uint32_t uiInsertID{0};
};

// A query and, once it ran, its results. Returned to callers by DirectQuery and PopResult;
// they delete it.
struct SQLMsg {
    // Collects every result set of the last query on sql.
    void Store(MYSQL *sql);

    // The first result set, or nullptr when the query produced none.
    auto Get() -> SQLResult * {
        return vec_pkResult.empty() ? nullptr : vec_pkResult.front().get();
    }

    int iID{0};
    std::string stQuery;
    std::vector<std::unique_ptr<SQLResult>> vec_pkResult;
    void *pvUserData{nullptr};
    bool bReturn{false}; // the caller collects the result with PopResult
    unsigned int uiSQLErrno{0};
};

// A MySQL connection. With a worker thread (the default) AsyncQuery/ReturnQuery queue queries
// that run in order on that thread; ReturnQuery results come back through PopResult. Without
// one (Setup with bNoThread) only DirectQuery is used, on the caller's thread.
class CAsyncSQL {
  public:
    CAsyncSQL() = default;
    CAsyncSQL(const CAsyncSQL &) = delete;
    auto operator=(const CAsyncSQL &) -> CAsyncSQL & = delete;
    ~CAsyncSQL();

    auto Setup(const char *c_pszHost, const char *c_pszUser, const char *c_pszPassword, const char *c_pszDB, const char *c_pszLocale, bool bNoThread = false, int iPort = 0) -> bool;
    void Quit();

    // Applies m_stLocale as the connection character set.
    auto QueryLocaleSet() -> bool;
    void SetLocale(const std::string &stLocale);

    void AsyncQuery(const char *c_pszQuery);
    void ReturnQuery(const char *c_pszQuery, void *pvUserData);
    auto DirectQuery(const char *c_pszQuery) -> SQLMsg *;
    auto PopResult(SQLMsg **pp) -> bool;

    auto CountQuery() -> uint32_t;
    auto CountResult() -> uint32_t;
    auto CountQueryFinished() -> int;
    void ResetQueryFinished();
    auto GetCopiedQueryCount() -> int;
    void ResetCopiedQueryCount();

    auto GetSQLHandle() -> MYSQL *;
    auto EscapeString(char *dst, size_t dstSize, const char *src, size_t srcSize) -> size_t;

  private:
    auto Connect() -> bool;
    void ChildLoop();
    auto NewMessage(const char *c_pszQuery) -> std::unique_ptr<SQLMsg>;
    void PushQuery(std::unique_ptr<SQLMsg> msg);
    void CheckReconnected();
    auto Execute(SQLMsg &msg) -> bool;
    void Finish(std::unique_ptr<SQLMsg> msg);

    MYSQL m_hDB{};
    bool m_bInitialized{false};

    std::string m_stHost;
    std::string m_stUser;
    std::string m_stPassword;
    std::string m_stDB;
    std::string m_stLocale;
    int m_iPort{0};

    std::atomic<int> m_iMsgCount{0};
    unsigned long m_ulThreadID{0};

    std::mutex m_mtxQuery;
    std::condition_variable m_cvQuery;
    std::deque<std::unique_ptr<SQLMsg>> m_queue_query;

    std::mutex m_mtxResult;
    std::deque<std::unique_ptr<SQLMsg>> m_queue_result;

    std::thread m_thread;
    std::atomic<bool> m_bEnd{false};

    std::atomic<int> m_iQueryFinished{0};
    std::atomic<int> m_iCopiedQuery{0};
};
}

// Used unqualified throughout game and db.
namespace metin2 {
    using sql::CAsyncSQL;
    using sql::SQLMsg;
    using sql::SQLResult;
}
