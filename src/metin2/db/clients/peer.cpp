#include <metin2/db/clients/peer.h>

#include <metin2/db/clients/client_manager.h>
#include <metin2/db/items/item_id_range_manager.h>

#include <metin2/log/log.h>
#include <metin2/net/connection.h>
#include <metin2/proto/tables.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <system_error>
#include <utility>

namespace metin2::db::inline clients {
    peer::peer(std::shared_ptr<net::connection> connection) : connection_(std::move(connection)) {
        input_consumed_ = 0;
        state_ = STATE_PLAYING;
        channel_ = 0;
        listen_port_ = 0;
        p2p_port_ = 0;

        memset(maps_, 0, sizeof(maps_));

        item_range_.min = item_range_.max = item_range_.usable_item_id_min = 0;
        item_spare_range_.min = item_spare_range_.max = item_spare_range_.usable_item_id_min = 0;

        static uint32_t current_handle = 0;
        handle_ = ++current_handle;

        metin2::log::info("Peer {} connected (handle {:d})", host(), handle_);
        connection_->start(*this);
    }

    peer::~peer() {
        connection_->close();

        metin2::log::info("Peer {} disconnected", host());
    }

    void peer::return_item_id_range() {
        metin2::log::info("Item id range: {:d} to {:d} returned to the pool", item_range_.min, item_range_.max);

        item_id_range_manager::instance().update_range(item_range_.min, item_range_.max);

        item_range_.min = 0;
        item_range_.max = 0;
        item_range_.usable_item_id_min = 0;
    }

    auto peer::on_receive(std::span<std::uint8_t> input, std::size_t) -> std::size_t {
        input_ = input;
        input_consumed_ = 0;
        client_manager::instance().process_packets(this);
        input_ = {};
        return input_consumed_;
    }

    void peer::on_close(const std::error_code &error) {
        if (error)
            metin2::log::warn("Lost connection to peer {}: {}", host(), error.message());

        state_ = STATE_CLOSE;
    }

    void peer::encode(const void *data, uint32_t size) {
        if (!connection_->send(data, size))
            metin2::log::error("Cannot send {:d} bytes to {}", size, host());
    }

    auto peer::handle() -> uint32_t {
        return handle_;
    }

    auto peer::peek_packet(int &bytes_proceed, uint8_t &header, uint32_t &handle, uint32_t &length, const char **data) -> bool {
        const int length_value = (int)input_.size();

        if (length_value < bytes_proceed + 9)
            return false;

        const char *buf = (const char *)input_.data();
        buf += bytes_proceed;

        header = *(buf++);

        handle = *((uint32_t *)buf);
        buf += sizeof(uint32_t);

        length = *((uint32_t *)buf);
        buf += sizeof(uint32_t);

        if (bytes_proceed + length + 9 > (uint32_t)length_value) {
            metin2::log::debug("Incomplete packet: need {:d} bytes, have {:d}",
                9 + length, length_value - bytes_proceed);
            return false;
        }

        *data = buf;
        bytes_proceed += length + 9;
        return true;
    }

    void peer::encode_header(uint8_t header, uint32_t handle_value, uint32_t size_value) {
        header_kind h;

        metin2::log::debug("Sending packet header {:d}, handle {:d}, size {:d}", header, handle_value, size_value);

        h.header = header;
        h.handle = handle_value;
        h.size = size_value;

        encode(&h, sizeof(header_kind));
    }

    void peer::encode_return(uint8_t header, uint32_t handle) {
        encode_header(header, handle, 0);
    }

    void peer::set_p2p_port(uint16_t port) {
        p2p_port_ = port;
    }

    void peer::set_maps(int32_t *pl) {
        std::memcpy(maps_, pl, sizeof(maps_));
    }

    void peer::send_spare_item_id_range() {
        if (item_spare_range_.min == 0 || item_spare_range_.max == 0 || item_spare_range_.usable_item_id_min == 0) {
            encode_header(HEADER_DG_ACK_SPARE_ITEM_ID_RANGE, 0, sizeof(item_id_range_table));
            encode(&item_spare_range_, sizeof(item_id_range_table));
        } else {
            set_item_id_range(item_spare_range_);

            if (set_spare_item_id_range(item_id_range_manager::instance().range()) == false) {
                metin2::log::error("Item id range: cannot assign spare range to peer {}", public_ip());
                item_spare_range_.min = item_spare_range_.max = item_spare_range_.usable_item_id_min = 0;
            }

            encode_header(HEADER_DG_ACK_SPARE_ITEM_ID_RANGE, 0, sizeof(item_id_range_table));
            encode(&item_spare_range_, sizeof(item_id_range_table));
        }
    }

    auto peer::set_item_id_range(item_id_range_table item_range) -> bool {
        if (item_range.min == 0 || item_range.max == 0 || item_range.usable_item_id_min == 0) return false;

        item_range_ = item_range;
        metin2::log::info("Item id range: assigned {1:d} to {2:d} to peer {0} (next free id {3:d})", public_ip(), item_range_.min, item_range_.max, item_range_.usable_item_id_min);

        return true;
    }

    auto peer::set_spare_item_id_range(item_id_range_table item_range) -> bool {
        if (item_range.min == 0 || item_range.max == 0 || item_range.usable_item_id_min == 0) return false;

        item_spare_range_ = item_range;
        metin2::log::info("Item id range: assigned spare {1:d} to {2:d} to peer {0} (next free id {3:d})", public_ip(), item_spare_range_.min, item_spare_range_.max,
            item_spare_range_.usable_item_id_min);

        return true;
    }

    auto peer::is_item_id_range_free(item_id_range_table item_range) -> bool {
        if (item_range_.min < item_range.max && item_range_.max > item_range.min) {
            metin2::log::error("Item id range: peer range {:d} to {:d} collides with {:d} to {:d}",
                item_range_.min, item_range_.max, item_range.min, item_range.max);
            return false;
        }

        if (item_spare_range_.min < item_range.max && item_spare_range_.max > item_range.min) {
            metin2::log::error("Item id range: peer spare range {:d} to {:d} collides with {:d} to {:d}",
                item_spare_range_.min, item_spare_range_.max, item_range.min, item_range.max);
            return false;
        }

        return true;
    }
}
