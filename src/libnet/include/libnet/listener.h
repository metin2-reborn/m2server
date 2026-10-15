#pragma once

#include <libnet/io.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace net {
    // Accepts TCP connections on an address and port and hands each new socket to a callback.
    class listener : public std::enable_shared_from_this<listener> {
      public:
        using accept_handler = std::function<void(tcp::socket)>;

        // Returns nullptr, after logging why, when the address cannot be bound.
        static auto open(const std::string &address, std::uint16_t port, accept_handler on_accept) -> std::shared_ptr<listener>;

        listener(tcp::acceptor acceptor, accept_handler on_accept);

        void close();

        // The bound port, which tells the port picked when listening on port 0.
        [[nodiscard]] auto port() const -> std::uint16_t;

      private:
        void accept();

        tcp::acceptor acceptor_;
        accept_handler on_accept_;
    };
}
