#pragma once

#include <metin2/proto/character_types.h>
#include <metin2/proto/tables.h>

#include <cstdint>

namespace metin2::db::inline login {

    class login_data {
      public:
        login_data();

        auto account_ref() -> account_table &;
        void set_client_key(const uint32_t *client_key);

        auto client_key() -> const uint32_t *;
        void set_key(uint32_t key);
        auto key() -> uint32_t;

        void set_connected_peer_handle(uint32_t handle);
        auto connected_peer_handle() -> uint32_t;

        void set_play(bool on);
        auto is_play() -> bool;

        void set_deleted(bool set);
        auto is_deleted() -> bool;

        void set_premium(int *pai_premium_times);
        auto premium_ptr() -> int *;

        auto last_player_id() const -> uint32_t { return last_player_id_; }
        void set_last_player_id(uint32_t id) { last_player_id_ = id; }

      private:
        uint32_t key_;
        uint32_t client_key_[4];
        uint32_t connected_peer_handle_;
        bool play_;
        bool deleted_;

        int premium_times_[PREMIUM_MAX_NUM];

        uint32_t last_player_id_;

        account_table data_;
    };
}
