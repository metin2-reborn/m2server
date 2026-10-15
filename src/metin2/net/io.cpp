#include <metin2/net/io.h>

#include <metin2/log/log.h>

#include <asio/buffer.hpp>
#include <asio/error.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <system_error>

#include <poll.h>
#include <sys/socket.h>

namespace metin2::net {
    namespace {
        std::uint64_t total_written = 0;

        // Waits for the socket to become readable or writable; false on timeout or error.
        auto wait(tcp::socket &socket, short events, std::chrono::steady_clock::time_point deadline) -> bool {
            for (;;) {
                const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
                if (left.count() <= 0) {
                    return false;
                }

                pollfd fd{.fd = socket.native_handle(), .events = events, .revents = 0};
                const int result = ::poll(&fd, 1, static_cast<int>(left.count()));
                if (result > 0) {
                    return true;
                }
                if (result < 0 && errno != EINTR) {
                    return false;
                }
            }
        }
    }

    auto io() -> asio::io_context & {
        static asio::io_context context(1);
        return context;
    }

    auto bytes_written() -> std::uint64_t {
        return total_written;
    }

    void detail::count_written(std::size_t size) {
        total_written += size;
    }

    auto connect(const std::string &host, std::uint16_t port, std::chrono::milliseconds timeout) -> std::optional<tcp::socket> {
        std::error_code error;
        tcp::resolver resolver(io());
        const auto endpoints = resolver.resolve(tcp::v4(), host, std::to_string(port), error);
        if (error) {
            metin2::log::error("cannot resolve {}: {}", host.c_str(), error.message().c_str());
            return std::nullopt;
        }

        const auto deadline = std::chrono::steady_clock::now() + timeout;
        for (const auto &entry : endpoints) {
            tcp::socket socket(io());
            socket.open(tcp::v4(), error);
            if (!error) socket.non_blocking(true, error);
            if (!error) socket.connect(entry.endpoint(), error);

            if (error == asio::error::in_progress || error == asio::error::would_block) {
                if (!wait(socket, POLLOUT, deadline)) {
                    error = asio::error::timed_out;
                } else {
                    int status = 0;
                    socklen_t length = sizeof(status);
                    ::getsockopt(socket.native_handle(), SOL_SOCKET, SO_ERROR, &status, &length);
                    error = std::error_code(status, asio::error::get_system_category());
                }
            }

            if (!error) {
                socket.set_option(tcp::socket::keep_alive(true), error);
                return socket;
            }
        }

        metin2::log::error("cannot connect to {}:{:d}: {}", host.c_str(), port, error.message().c_str());
        return std::nullopt;
    }

    auto write(tcp::socket &socket, std::span<const std::uint8_t> data, std::chrono::milliseconds timeout) -> bool {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        std::error_code error;
        socket.non_blocking(true, error);

        while (!data.empty() && !error) {
            const std::size_t size = socket.write_some(asio::buffer(data.data(), data.size()), error);
            if (error == asio::error::would_block) {
                error.clear();
                if (!wait(socket, POLLOUT, deadline)) {
                    return false;
                }
                continue;
            }
            detail::count_written(size);
            data = data.subspan(size);
        }
        return !error;
    }

    auto read_some(tcp::socket &socket, std::span<std::uint8_t> buffer, std::chrono::milliseconds timeout) -> std::size_t {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        std::error_code error;
        socket.non_blocking(true, error);

        while (!error) {
            const std::size_t size = socket.read_some(asio::buffer(buffer.data(), buffer.size()), error);
            if (error != asio::error::would_block) {
                return error ? 0 : size;
            }
            error.clear();
            if (!wait(socket, POLLIN, deadline)) {
                return 0;
            }
        }
        return 0;
    }
}
