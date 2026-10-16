#pragma once

#include <metin2/net/connection.h>
#include <metin2/proto/tables.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <system_error>

namespace metin2::db::inline clients {

    class peer : public net::connection_handler {
      public:
#pragma pack(1)
        struct header_kind {
            uint8_t header;
            uint32_t handle;
            uint32_t size;
        };
#pragma pack()
        enum state {
            STATE_CLOSE = 0,
            STATE_PLAYING = 1
        };

        explicit peer(std::shared_ptr<net::connection> connection);
        ~peer() override;

        auto on_receive(std::span<std::uint8_t> input, std::size_t received) -> std::size_t override;
        void on_close(const std::error_code &error) override;

        auto is_closed() const -> bool { return state_ == STATE_CLOSE; }
        auto host() const -> const char * { return connection_->remote_address().c_str(); }

        void encode_byte(uint8_t b) { encode(&b, sizeof(b)); }
        void encode_word(uint16_t w) { encode(&w, sizeof(w)); }
        void encode_dword(uint32_t dw) { encode(&dw, sizeof(dw)); }
        void encode(const void *data, uint32_t size);

        void encode_header(uint8_t header, uint32_t handle_value, uint32_t size_value);
        auto peek_packet(int &bytes_proceed, uint8_t &header, uint32_t &handle, uint32_t &length, const char **data) -> bool;
        void encode_return(uint8_t header, uint32_t handle);
        void recv_end(int bytes_proceed) { input_consumed_ = bytes_proceed; }

        auto handle() -> uint32_t;
        void set_user_count(uint32_t count);

        void set_public_ip(const char *ip) { public_ip_ = ip; }
        auto public_ip() -> const char * { return public_ip_.c_str(); }

        void set_channel(uint8_t channel) { channel_ = channel; }
        auto channel() -> uint8_t { return channel_; }

        void set_listen_port(uint16_t port) { listen_port_ = port; }
        auto listen_port() -> uint16_t { return listen_port_; }

        void set_p2p_port(uint16_t port);
        auto p2p_port() -> uint16_t { return p2p_port_; }

        void set_maps(int32_t *pl);
        auto maps() -> int32_t * { return &maps_[0]; }

        auto set_item_id_range(item_id_range_table item_range) -> bool;
        auto set_spare_item_id_range(item_id_range_table item_range) -> bool;
        auto is_item_id_range_free(item_id_range_table item_range) -> bool;
        void send_spare_item_id_range();
        void return_item_id_range();

      private:
        std::shared_ptr<net::connection> connection_;
        std::span<const std::uint8_t> input_;
        std::size_t input_consumed_;

        int state_;

        uint8_t channel_;
        uint32_t handle_;
        uint16_t listen_port_;
        uint16_t p2p_port_;
        int32_t maps_[MAX_MAPS_PER_SERVER];

        item_id_range_table item_range_;
        item_id_range_table item_spare_range_;

        std::string public_ip_;
    };
}
