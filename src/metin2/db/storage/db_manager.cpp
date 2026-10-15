#include <metin2/db/storage/db_manager.h>

#include <metin2/db/clients/client_manager.h>

#include <metin2/sql/async_sql.h>
#include <metin2/log/log.h>

#include <mysql.h>

#include <cassert>
#include <cstdint>
#include <memory>

namespace metin2::db::inline storage {

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

auto db_manager::pop_result() -> sql_msg * {
    sql_msg *p;

    for (int i = 0; i < SQL_MAX_NUM; ++i)
        if (main_sql_[i] && main_sql_[i]->pop_result(&p))
            return p;

    return nullptr;
}

auto db_manager::connect(int slot, const char *db_address, const int db_port, const char *db_name, const char *user, const char *pwd) -> int {
    if (db_address == nullptr || db_name == nullptr)
        return false;

    if (slot < 0 || slot >= SQL_MAX_NUM)
        return false;

    metin2::log::info( "CREATING DIRECT_SQL");
    direct_sql_[slot] = std::make_unique<async_sql>();
    if (!direct_sql_[slot]->setup(db_address, user, pwd, db_name, CONNECTION_CHARSET, true, db_port)) {
        clear();
        return false;
    }

    metin2::log::info( "CREATING MAIN_SQL");
    main_sql_[slot] = std::make_unique<async_sql>();
    if (!main_sql_[slot]->setup(db_address, user, pwd, db_name, CONNECTION_CHARSET, false, db_port)) {
        clear();
        return false;
    }

    metin2::log::info( "CREATING ASYNC_SQL");
    async_sql_[slot] = std::make_unique<async_sql>();
    if (!async_sql_[slot]->setup(db_address, user, pwd, db_name, CONNECTION_CHARSET, false, db_port)) {
        clear();
        return false;
    }

    return true;
}

auto db_manager::direct_query(const char *query, int slot) -> sql_msg * {
    return direct_sql_[slot]->direct_query(query);
}

void db_manager::return_query(const char *query, int type_value, uint32_t ident_value, void *udata, int slot) {
    assert(slot < SQL_MAX_NUM);
    query_info *p = new query_info;

    p->type = type_value;
    p->ident = ident_value;
    p->pv_data = udata;

    main_sql_[slot]->return_query(query, p);

    ++query_count[0];
}

void db_manager::async_query(const char *query, int slot) {
    assert(slot < SQL_MAX_NUM);
    async_sql_[slot]->async_query(query);
    ++query_count[1];
}

auto db_manager::escape_string(void *to, const void *from, uint32_t length, int slot) -> uint32_t {
    assert(slot < SQL_MAX_NUM);
    return mysql_real_escape_string(direct_sql_[slot]->sql_handle(), (char *)to, (const char *)from, length);
}
}
