#pragma once

#include <metin2/game/app/typedef.h>

#include <metin2/common/singleton.h>
#include <metin2/net/io.h>
#include <metin2/proto/length.h>

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace metin2::game::inline protocol {

    class login_key;

    class desc_manager : public singleton<desc_manager> {
      public:
        typedef std::unordered_set<desc *> desc_set;
        typedef std::unordered_set<client_desc *> client_desc_set;
        typedef std::map<int, desc *> desc_handle_map;
        typedef std::map<uint32_t, desc *> desc_handshake_map;
        typedef std::unordered_map<std::string, desc *> desc_loginname_map;
        typedef std::map<uint32_t, uint32_t> desc_handle_random_key_map;

      public:
        desc_manager();
        ~desc_manager() override;

        void initialize();
        void destroy();

        auto accept_desc(net::tcp::socket socket) -> desc *;
        auto accept_p2_p_desc(net::tcp::socket socket) -> desc *;
        void destroy_desc(desc *d, bool erase_from_set = true);

        auto create_handshake() -> uint32_t;

        auto create_connection_desc(const char *host, uint16_t port, int phase_when_succeed, bool retry_when_closed) -> client_desc *;
        void try_connect();

        auto find_by_handle(uint32_t handle) -> desc *;

        auto find_by_character_name(const char *name) -> desc *;
        auto find_by_login_name(const std::string &login) -> desc *;
        void connect_account(const std::string &login, desc *d);
        void disconnect_account(const std::string &login);

        void destroy_closed();

        void update_local_user_count();
        auto local_user_count() -> uint32_t { return local_user_count_; }
        void user_count(int &total, int **pai_empire_user_count, int &local_count);

        auto client_set() -> const desc_set &;

        auto make_random_key(uint32_t handle) -> uint32_t;
        auto check_random_key(uint32_t handle, uint32_t key) -> bool;

        auto create_login_key(desc *d) -> uint32_t;
        auto find_by_login_key(uint32_t key) -> desc *;
        void process_expired_login_key();

        auto is_p2_p_desc_exist(const char *host, uint16_t port) -> bool;

      private:
        desc_handle_random_key_map map_handle_random_key_;

        client_desc_set set_pk_client_desc_;
        desc_set set_pk_desc_;

        desc_handle_map map_handle_;
        desc_handshake_map map_handshake_;
        desc_loginname_map map_login_name_;
        std::map<uint32_t, login_key *> map_pk_login_key_;

        int sockets_connected_;

        int handle_count_;

        int local_user_count_;
        int empire_user_count_[EMPIRE_MAX_NUM];

        bool destroyed_;
    };
}
