#include <metin2/db/login/login_data.h>

#include <metin2/log/log.h>
#include <metin2/proto/tables.h>

#include <cstdint>
#include <cstring>

namespace metin2::db::inline login {

    login_data::login_data() {
        key_ = 0;
        memset(client_key_, 0, sizeof(client_key_));
        connected_peer_handle_ = 0;
        play_ = false;
        deleted_ = false;
        last_player_id_ = 0;

        memset(&data_, 0, sizeof(account_table));
    }

    auto login_data::account_ref() -> account_table & {
        return data_;
    }

    void login_data::set_client_key(const uint32_t *client_key) {
        std::memcpy(&client_key_, client_key, sizeof(uint32_t) * 4);
    }

    auto login_data::client_key() -> const uint32_t * {
        return &client_key_[0];
    }

    void login_data::set_key(uint32_t key) {
        key_ = key;
    }

    auto login_data::key() -> uint32_t {
        return key_;
    }

    void login_data::set_connected_peer_handle(uint32_t handle) {
        connected_peer_handle_ = handle;
    }

    auto login_data::connected_peer_handle() -> uint32_t {
        return connected_peer_handle_;
    }

    void login_data::set_play(bool on) {
        metin2::log::debug("Login: playing flag {} for key {:d}, account {}", on ? "on" : "off", key(), data_.login);
        play_ = on;
    }

    auto login_data::is_play() -> bool {
        return play_;
    }

    void login_data::set_deleted(bool set) {
        deleted_ = set;
    }

    auto login_data::is_deleted() -> bool {
        return deleted_;
    }

    void login_data::set_premium(int *pai_premium_times) {
        std::memcpy(premium_times_, pai_premium_times, sizeof(premium_times_));
    }

    auto login_data::premium_ptr() -> int * {
        return &premium_times_[0];
    }
}
