#pragma once

#include <metin2/game/app/typedef.h>

#include <metin2/common/singleton.h>
#include <metin2/sql/async_sql.h>

#include <cstdint>
#include <map>
#include <set>
#include <string>

namespace metin2::game::inline social {

    class messenger_manager : public singleton<messenger_manager> {
      public:
        typedef std::string key_t;
        typedef const std::string &key_a;

        messenger_manager();
        ~messenger_manager() override;

      public:
        void p2_p_login(key_a account);
        void p2_p_logout(key_a account);

        void login(key_a account);
        void logout(key_a account);

        void request_to_add(character *ch, character *target);
        auto auth_to_add(key_a account, key_a companion, bool deny) -> bool;

        void apply_add_to_list(key_a account, key_a companion);
        void add_to_list(key_a account, key_a companion);

        void apply_remove_from_list(key_a account, key_a companion);
        void remove_from_list(key_a account, key_a companion);

        void remove_all_list(key_a account);

        void initialize();

        auto is_in_list(messenger_manager::key_a account, messenger_manager::key_a companion) -> bool;

      private:
        void send_list(key_a account);
        void send_login(key_a account, key_a companion);
        void send_logout(key_a account, key_a companion);

        void load_list(sql_msg *msg);

        void destroy();

        std::set<key_t> set_login_account_;
        std::map<key_t, std::set<key_t>> relation_;
        std::map<key_t, std::set<key_t>> inverse_relation_;
        std::set<uint32_t> set_request_to_add_;
    };
}
