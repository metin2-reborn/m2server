#pragma once

#include <metin2/net/io.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <system_error>
#include <vector>

namespace metin2::net {
    class connection_handler {
      public:
        virtual ~connection_handler() = default;

        virtual auto on_receive(std::span<std::uint8_t> input, std::size_t received) -> std::size_t = 0;

        virtual void on_close(const std::error_code &error) = 0;
    };

    // Owned through a shared_ptr; pending handlers keep it alive until they have run.
    class connection : public std::enable_shared_from_this<connection> {
      public:
        static auto create(tcp::socket socket, std::size_t max_input, std::size_t max_output) -> std::shared_ptr<connection>;

        connection(tcp::socket socket, std::size_t max_input, std::size_t max_output);

        connection(const connection &) = delete;
        auto operator=(const connection &) -> connection & = delete;

        // The handler must outlive the connection, or call close() first.
        void start(connection_handler &handler);

        void close();

        [[nodiscard]] auto is_open() const -> bool { return socket_.is_open(); }

        auto send(std::span<const std::uint8_t> data) -> bool;
        auto send(const void *data, std::size_t size) -> bool;

        auto prepare(std::size_t size) -> std::span<std::uint8_t>;

        // The next write may exceed the output limit, but only onto an empty queue: a peer that does not read
        // cannot pile up oversized replies.
        void allow_large_write() { large_write_allowed_ = true; }

        [[nodiscard]] auto pending_output() const -> std::size_t { return output_.size() - output_sent_; }

        // For data that must not share a segment with the output queued after it.
        void write_now() { write(); }

        auto flush(std::chrono::milliseconds timeout) -> bool;

        [[nodiscard]] auto remote_address() const -> const std::string & { return address_; }
        [[nodiscard]] auto remote_port() const -> std::uint16_t { return port_; }

      private:
        void read();
        void on_read(const std::error_code &error, std::size_t size);
        void schedule_write();
        void write();
        void fail(const std::error_code &error);

        tcp::socket socket_;
        std::string address_;
        std::uint16_t port_{0};
        connection_handler *handler_{nullptr};

        std::vector<std::uint8_t> input_;
        std::size_t input_size_{0};
        std::size_t max_input_;

        std::vector<std::uint8_t> output_;
        std::size_t output_sent_{0};
        std::size_t max_output_;
        bool write_scheduled_{false};
        bool large_write_allowed_{false};
        bool waiting_writable_{false};
    };
}
