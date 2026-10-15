#pragma once

#include <common/singleton.h>
#include <libsql/AsyncSQL.h>

#include <cstdint>
#include <memory>

class CQueryInfo {
  public:
    int iType;
    uint32_t dwIdent;
    void *pvData;
};

enum eSQL_SLOT {
    SQL_PLAYER,
    SQL_ACCOUNT,
    SQL_COMMON,
    SQL_MAX_NUM,
};

class CDBManager : public singleton<CDBManager> {
    std::unique_ptr<CAsyncSQL> m_mainSQL[SQL_MAX_NUM];
    std::unique_ptr<CAsyncSQL> m_directSQL[SQL_MAX_NUM];
    std::unique_ptr<CAsyncSQL> m_asyncSQL[SQL_MAX_NUM];

  protected:
    void Initialize();
    void Destroy();

  public:
    CDBManager();

    ~CDBManager() override;

    void Clear();
    void Quit();

    auto Connect(int iSlot, const char *host, int port, const char *dbname, const char *user, const char *pass) -> int;

    void ReturnQuery(const char *c_pszQuery, int iType, uint32_t dwIdent, void *pvData, int iSlot = SQL_PLAYER);
    void AsyncQuery(const char *c_pszQuery, int iSlot = SQL_PLAYER);
    auto DirectQuery(const char *c_pszQuery, int iSlot = SQL_PLAYER) -> SQLMsg *;

    auto PopResult() -> SQLMsg *;

    auto EscapeString(void *to, const void *from, uint32_t length, int iSlot = SQL_PLAYER) -> uint32_t;

    auto CountReturnQuery(int i) -> uint32_t { return m_mainSQL[i] ? m_mainSQL[i]->CountQuery() : 0; }
    auto CountReturnResult(int i) -> uint32_t { return m_mainSQL[i] ? m_mainSQL[i]->CountResult() : 0; }
    auto CountReturnQueryFinished(int i) -> uint32_t { return m_mainSQL[i] ? m_mainSQL[i]->CountQueryFinished() : 0; }
    auto CountReturnCopiedQuery(int i) -> uint32_t { return m_mainSQL[i] ? m_mainSQL[i]->GetCopiedQueryCount() : 0; }

    auto CountAsyncQuery(int i) -> uint32_t { return m_asyncSQL[i] ? m_asyncSQL[i]->CountQuery() : 0; }
    auto CountAsyncResult(int i) -> uint32_t { return m_asyncSQL[i] ? m_asyncSQL[i]->CountResult() : 0; }
    auto CountAsyncQueryFinished(int i) -> uint32_t { return m_asyncSQL[i] ? m_asyncSQL[i]->CountQueryFinished() : 0; }
    auto CountAsyncCopiedQuery(int i) -> uint32_t { return m_asyncSQL[i] ? m_asyncSQL[i]->GetCopiedQueryCount() : 0; }

    void ResetCounter() {
        for (int i = 0; i < SQL_MAX_NUM; ++i) {
            if (m_mainSQL[i]) {
                m_mainSQL[i]->ResetQueryFinished();
                m_mainSQL[i]->ResetCopiedQueryCount();
            }

            if (m_asyncSQL[i]) {
                m_asyncSQL[i]->ResetQueryFinished();
                m_asyncSQL[i]->ResetCopiedQueryCount();
            }
        }
    }
};
