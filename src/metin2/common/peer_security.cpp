#include <metin2/common/peer_security.h>

#include <asio/io_context.hpp>
#include <asio/ip/address_v4.hpp>
#include <asio/ip/tcp.hpp>

#include <algorithm>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

namespace metin2::inline common {

    auto is_allowed_peer(std::string_view address, std::span<const std::string> hosts, std::string_view local) -> bool {
        std::error_code error;
        const asio::ip::address remote = asio::ip::make_address_v4(address, error);

        if (error) {
            return false;
        }

        if (hosts.empty()) {
            return address == local;
        }

        static asio::io_context io;
        asio::ip::tcp::resolver resolver(io);

        return std::ranges::any_of(hosts, [&](const std::string &host) -> bool {
            const auto endpoints = resolver.resolve(asio::ip::tcp::v4(), host, "", error);
            return !error && std::ranges::any_of(endpoints, [&](const auto &entry) -> bool { return entry.endpoint().address() == remote; });
        });
    }
}
