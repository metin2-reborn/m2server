#include "DBManager.h"

#include "ClientManager.h"

#include <common/tables.h>
#include <libsql/AsyncSQL.h>
#include <libthecore/log.h>

#include <mysql.h>

#include <cassert>
#include <cstdint>

// Character set of every connection; the game server connects with the same one.
static constexpr const char CONNECTION_CHARSET[] = "latin1";

CDBManager::CDBManager() {
    Initialize();
}

CDBManager::~CDBManager() {
    Destroy();
}

void CDBManager::Initialize() {
    for (int i = 0; i < SQL_MAX_NUM; ++i) {
        m_mainSQL[i] = nullptr;
        m_directSQL[i] = nullptr;
        m_asyncSQL[i] = nullptr;
    }
}

void CDBManager::Destroy() {
    Clear();
}

void CDBManager::Clear() {
    for (int i = 0; i < SQL_MAX_NUM; ++i) {
        if (m_mainSQL[i]) {
            delete m_mainSQL[i];
            m_mainSQL[i] = nullptr;
        }

        if (m_directSQL[i]) {
            delete m_directSQL[i];
            m_directSQL[i] = nullptr;
        }

        if (m_asyncSQL[i]) {
            delete m_asyncSQL[i];
            m_asyncSQL[i] = nullptr;
        }
    }

    Initialize();
}

void CDBManager::Quit() {
    for (int i = 0; i < SQL_MAX_NUM; ++i) {
        if (m_mainSQL[i])
            m_mainSQL[i]->Quit();

        if (m_asyncSQL[i])
            m_asyncSQL[i]->Quit();

        if (m_directSQL[i])
            m_directSQL[i]->Quit();
    }
}

SQLMsg *CDBManager::PopResult() {
    SQLMsg *p;

    for (int i = 0; i < SQL_MAX_NUM; ++i)
        if (m_mainSQL[i] && m_mainSQL[i]->PopResult(&p))
            return p;

    return nullptr;
}

int CDBManager::Connect(int iSlot, const char *db_address, const int db_port, const char *db_name, const char *user, const char *pwd) {
    if (db_address == nullptr || db_name == nullptr)
        return false;

    if (iSlot < 0 || iSlot >= SQL_MAX_NUM)
        return false;

    sys_log(0, "CREATING DIRECT_SQL");
    m_directSQL[iSlot] = new CAsyncSQL;
    if (!m_directSQL[iSlot]->Setup(db_address, user, pwd, db_name, CONNECTION_CHARSET, true, db_port)) {
        Clear();
        return false;
    }

    sys_log(0, "CREATING MAIN_SQL");
    m_mainSQL[iSlot] = new CAsyncSQL;
    if (!m_mainSQL[iSlot]->Setup(db_address, user, pwd, db_name, CONNECTION_CHARSET, false, db_port)) {
        Clear();
        return false;
    }

    sys_log(0, "CREATING ASYNC_SQL");
    m_asyncSQL[iSlot] = new CAsyncSQL;
    if (!m_asyncSQL[iSlot]->Setup(db_address, user, pwd, db_name, CONNECTION_CHARSET, false, db_port)) {
        Clear();
        return false;
    }

    return true;
}

SQLMsg *CDBManager::DirectQuery(const char *c_pszQuery, int iSlot) {
    return m_directSQL[iSlot]->DirectQuery(c_pszQuery);
}

void CDBManager::ReturnQuery(const char *c_pszQuery, int iType, IDENT dwIdent, void *udata, int iSlot) {
    assert(iSlot < SQL_MAX_NUM);
    CQueryInfo *p = new CQueryInfo;

    p->iType = iType;
    p->dwIdent = dwIdent;
    p->pvData = udata;

    m_mainSQL[iSlot]->ReturnQuery(c_pszQuery, p);

    ++g_query_count[0];
}

void CDBManager::AsyncQuery(const char *c_pszQuery, int iSlot) {
    assert(iSlot < SQL_MAX_NUM);
    m_asyncSQL[iSlot]->AsyncQuery(c_pszQuery);
    ++g_query_count[1];
}

uint32_t CDBManager::EscapeString(void *to, const void *from, uint32_t length, int iSlot) {
    assert(iSlot < SQL_MAX_NUM);
    return mysql_real_escape_string(m_directSQL[iSlot]->GetSQLHandle(), (char *)to, (const char *)from, length);
}
