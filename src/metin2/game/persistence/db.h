#pragma once

#include <metin2/game/app/typedef.h>

#include <metin2/common/singleton.h>
#include <metin2/sql/async_sql.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace metin2::game::inline protocol {
    class login_data;
}

namespace metin2::game::inline persistence {

    enum {
        QUERY_TYPE_RETURN = 1,
        QUERY_TYPE_FUNCTION = 2,
        QUERY_TYPE_AFTER_FUNCTION = 3,
    };

    enum {
        QID_SAFEBOX_SIZE,
        QID_DB_STRING,
        QID_AUTH_LOGIN,
        QID_LOTTO,
        QID_HIGHSCORE_REGISTER,
        QID_HIGHSCORE_SHOW,

        QID_BLOCK_CHAT_LIST,

        QID_PROTECT_CHILD,

    };

    class query_info {
      public:
        int query_type;
    };

    class return_query_info : public query_info {
      public:
        int type;
        uint32_t ident;
        void *pv_data;
    };

    class func_query_info : public query_info {
      public:
        std::function<void(sql_msg *)> f;
    };

    class func_after_query_info : public query_info {
      public:
        std::function<void()> f;
    };

    class db_manager : public singleton<db_manager> {
      public:
        db_manager();
        ~db_manager() override;

        auto is_connected() -> bool;

        auto connect(const char *host, const int port, const char *user, const char *pwd, const char *db) -> bool;
        void query(const std::string &query);
        void query(const char *format, ...) __attribute__((format(printf, 2, 3)));

        auto direct_query(const std::string &query) -> sql_msg *;
        __attribute__((format(printf, 2, 3))) auto direct_query(const char *format, ...) -> sql_msg *;
        void return_query(int type_value, uint32_t ident_value, void *data, const std::string &query);
        void return_query(int type, uint32_t ident, void *data, const char *format, ...) __attribute__((format(printf, 5, 6)));

        void process();
        void analyze_return_query(sql_msg *msg_value);

        void send_money_log(uint8_t type, uint32_t vnum, int gold);

        void login_prepare(desc *d, uint32_t *client_key, int *pai_premium_times);
        void send_auth_login(desc *d);

        void insert_login_data(std::unique_ptr<login_data> ld);
        void delete_login_data(login_data *ld);
        auto get_login_data(uint32_t key) -> login_data *;

        auto count_query() -> uint32_t { return sql_.count_query(); }

        void load_db_string();
        auto greet_message() -> const std::vector<std::string> &;

        void func_query(std::function<void(sql_msg *)> f, const std::string &query);
        void func_query(std::function<void(sql_msg *)> f, const char *format, ...) __attribute__((format(printf, 3, 4)));
        void func_after_query(std::function<void()> f, const std::string &query);
        void func_after_query(std::function<void()> f, const char *format, ...) __attribute__((format(printf, 3, 4)));

        auto escape_string(char *dst, size_t dst_size, const char *src, size_t src_size) -> size_t;

      private:
        auto pop_result() -> sql_msg *;

        async_sql sql_;
        async_sql sql_direct_;
        bool is_connect_;

        std::map<std::string, std::string> map_dbstring_;
        std::vector<std::string> vec_greet_message_;
        std::map<uint32_t, std::unique_ptr<login_data>> map_pk_login_data_;
    };

    struct highscore_register_query_info {
        char board[20 + 1];
        uint32_t pid;
        int value;
        bool order;
    };

    class account_db : public singleton<account_db> {
      public:
        account_db();

        auto is_connected() -> bool;
        auto connect(const char *host, const int port, const char *user, const char *pwd, const char *db) -> bool;
        auto connect_async(const char *host, const int port, const char *user, const char *pwd, const char *db, const char *locale) -> bool;

        auto direct_query(const char *query) -> sql_msg *;
        auto direct_query(const std::string &query) -> sql_msg * { return direct_query(query.c_str()); }
        void return_query(int type_value, uint32_t ident_value, void *data, const std::string &query);
        void return_query(int type, uint32_t ident, void *data, const char *format, ...) __attribute__((format(printf, 5, 6)));

        void set_locale(const std::string &locale);

        void process();

      private:
        auto pop_result() -> sql_msg *;
        void analyze_return_query(sql_msg *msg);

        async_sql sql_direct_;
        async_sql sql_;
        bool is_connect_;
    };

    extern void cancel_reload_spam_event();
    extern void load_spam_db();
}
