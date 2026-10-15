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

    // The process's event loop. It runs on the main thread only: the server's pulse loop polls it,
    // so every network handler runs between pulses like the rest of the game logic.
    auto io() -> asio::io_context &;

    // Bytes written to all connections since start.
    auto bytes_written() -> std::uint64_t;

    namespace detail {
        void count_written(std::size_t size);
    }

    // Connects to host:port (a name or an IPv4 address), giving up after `timeout`. Failures are
    // logged.
    auto connect(const std::string &host, std::uint16_t port, std::chrono::milliseconds timeout) -> std::optional<tcp::socket>;

    // Blocking transfers on a connected socket that give up after `timeout`.
    auto write(tcp::socket &socket, std::span<const std::uint8_t> data, std::chrono::milliseconds timeout) -> bool;
    auto read_some(tcp::socket &socket, std::span<std::uint8_t> buffer, std::chrono::milliseconds timeout) -> std::size_t;
}
