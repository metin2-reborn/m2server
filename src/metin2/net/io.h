#pragma once

#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace metin2::net {
    using tcp = asio::ip::tcp;

    auto io() -> asio::io_context &;

    auto bytes_written() -> std::uint64_t;

    namespace detail {
        void count_written(std::size_t size);
    }

    auto connect(const std::string &host, std::uint16_t port, std::chrono::milliseconds timeout) -> std::optional<tcp::socket>;

    auto write(tcp::socket &socket, std::span<const std::uint8_t> data, std::chrono::milliseconds timeout) -> bool;
    auto read_some(tcp::socket &socket, std::span<std::uint8_t> buffer, std::chrono::milliseconds timeout) -> std::size_t;
}
