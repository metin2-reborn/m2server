#pragma once

#include <metin2/net/io.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace metin2::net {
    class listener : public std::enable_shared_from_this<listener> {
      public:
        using accept_handler = std::function<void(tcp::socket)>;

        static auto open(const std::string &address, std::uint16_t port, accept_handler on_accept) -> std::shared_ptr<listener>;

        listener(tcp::acceptor acceptor, accept_handler on_accept);

        void close();

        [[nodiscard]] auto port() const -> std::uint16_t;

      private:
        void accept();

        tcp::acceptor acceptor_;
        accept_handler on_accept_;
    };
}
