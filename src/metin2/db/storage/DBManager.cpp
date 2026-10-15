#include <metin2/db/storage/DBManager.h>

#include <metin2/db/clients/ClientManager.h>

#include <metin2/proto/tables.h>
#include <metin2/sql/AsyncSQL.h>
#include <metin2/log/log.h>

#include <mysql.h>

#include <cassert>
#include <cstdint>
#include <memory>

namespace metin2::db::inline storage {

// Character set of every connection; the game server connects with the same one.
static constexpr const char CONNECTION_CHARSET[] = "latin1";

db_manager::db_manager() {
    initialize();
}

db_manager::~db_manager() {
    destroy();
}

void db_manager::initialize() {
    for (int i = 0; i < SQL_MAX_NUM; ++i) {
        main_sql_[i] = nullptr;
        direct_sql_[i] = nullptr;
        async_sql_[i] = nullptr;
    }
}

void db_manager::destroy() {
    clear();
}

void db_manager::clear() {
    for (int i = 0; i < SQL_MAX_NUM; ++i) {
        main_sql_[i].reset();
        direct_sql_[i].reset();
        async_sql_[i].reset();
    }

    initialize();
}

void db_manager::quit() {
    for (int i = 0; i < SQL_MAX_NUM; ++i) {
        if (main_sql_[i])
            main_sql_[i]->quit();

        if (async_sql_[i])
            async_sql_[i]->quit();

        if (direct_sql_[i])
            direct_sql_[i]->quit();
    }
}

ql_msg *db_manager::pop_result() {
    ql_msg *p;

    for (int i = 0; i < SQL_MAX_NUM; ++i)
        if (main_sql_[i] && main_sql_[i]->pop_result(&p))
            return p;

    return nullptr;
}

int db_manager::connect(int iSlot, const char *db_address, const int db_port, const char *db_name, const char *user, const char *pwd) {
    if (db_address == nullptr || db_name == nullptr)
        return false;

    if (iSlot < 0 || iSlot >= SQL_MAX_NUM)
        return false;

    metin2::log::info( "CREATING DIRECT_SQL");
    direct_sql_[iSlot] = std::make_unique<async_sql>();
    if (!direct_sql_[iSlot]->setup(db_address, user, pwd, db_name, CONNECTION_CHARSET, true, db_port)) {
        clear();
        return false;
    }

    metin2::log::info( "CREATING MAIN_SQL");
    main_sql_[iSlot] = std::make_unique<async_sql>();
    if (!main_sql_[iSlot]->setup(db_address, user, pwd, db_name, CONNECTION_CHARSET, false, db_port)) {
        clear();
        return false;
    }

    metin2::log::info( "CREATING ASYNC_SQL");
    async_sql_[iSlot] = std::make_unique<async_sql>();
    if (!async_sql_[iSlot]->setup(db_address, user, pwd, db_name, CONNECTION_CHARSET, false, db_port)) {
        clear();
        return false;
    }

    return true;
}

ql_msg *db_manager::direct_query(const char *c_pszQuery, int iSlot) {
    return direct_sql_[iSlot]->direct_query(c_pszQuery);
}

void db_manager::return_query(const char *c_pszQuery, int iType, uint32_t dwIdent, void *udata, int iSlot) {
    assert(iSlot < SQL_MAX_NUM);
    query_info *p = new query_info;

    p->type = iType;
    p->ident = dwIdent;
    p->pv_data = udata;

    main_sql_[iSlot]->return_query(c_pszQuery, p);

    ++query_count[0];
}

void db_manager::async_query(const char *c_pszQuery, int iSlot) {
    assert(iSlot < SQL_MAX_NUM);
    async_sql_[iSlot]->async_query(c_pszQuery);
    ++query_count[1];
}

uint32_t db_manager::escape_string(void *to, const void *from, uint32_t length, int iSlot) {
    assert(iSlot < SQL_MAX_NUM);
    return mysql_real_escape_string(direct_sql_[iSlot]->sql_handle(), (char *)to, (const char *)from, length);
}
}
