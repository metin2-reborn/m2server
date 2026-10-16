#pragma once

#include <metin2/game/app/typedef.h>

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

    class db_manager {
      public:
        db_manager();
        ~db_manager();

        auto is_connected() -> bool;

        auto connect(const char *host, const int port, const char *user, const char *pwd, const char *db) -> bool;
        void query(const std::string &query);

        auto direct_query(const std::string &query) -> std::unique_ptr<sql_msg>;
        void process();

        void send_money_log(uint8_t type, uint32_t vnum, int gold);

        void login_prepare(desc *d, uint32_t *client_key, int *pai_premium_times);
        void send_auth_login(desc *d);

        void insert_login_data(std::unique_ptr<login_data> ld);
        void delete_login_data(login_data *ld);
        auto get_login_data(uint32_t key) -> login_data *;

        auto count_query() -> uint32_t { return static_cast<uint32_t>(sql_.stats().queued); }

        void load_db_string();
        auto greet_message() -> const std::vector<std::string> &;

        void func_query(sql_callback f, const std::string &query);
        void func_after_query(std::move_only_function<void()> f, const std::string &query);

      private:
        auto pop_result() -> std::unique_ptr<sql_msg>;

        async_sql sql_;
        async_sql sql_direct_;
        bool is_connect_;

        std::map<std::string, std::string> map_dbstring_;
        std::vector<std::string> vec_greet_message_;
        std::map<uint32_t, std::unique_ptr<login_data>> map_login_data_;
    };

    inline auto get_db_manager() -> db_manager & {
        static db_manager instance;
        return instance;
    }

    class account_db {
      public:
        account_db();

        auto is_connected() -> bool;
        auto connect(const char *host, const int port, const char *user, const char *pwd, const char *db) -> bool;
        auto connect_async(const char *host, const int port, const char *user, const char *pwd, const char *db, const char *locale) -> bool;

        auto direct_query(const char *query) -> std::unique_ptr<sql_msg>;
        auto direct_query(const std::string &query) -> std::unique_ptr<sql_msg> { return direct_query(query.c_str()); }
        void func_query(sql_callback f, const std::string &query);

        void set_locale(const std::string &locale);

        void process();

      private:
        auto pop_result() -> std::unique_ptr<sql_msg>;

        async_sql sql_direct_;
        async_sql sql_;
        bool is_connect_;
    };

    inline auto get_account_db() -> account_db & {
        static account_db instance;
        return instance;
    }

    extern void cancel_reload_spam_event();
    extern void load_spam_db();
    extern void give_lotto_ticket_result(sql_msg &msg, uint32_t pid, uint32_t vnum, uint32_t count, uint32_t round);
}
