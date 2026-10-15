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
    // Receives a connection's input and its end.
    class connection_handler {
      public:
        virtual ~connection_handler() = default;

        // Called with all unconsumed input, of which the last `received` bytes just arrived.
        // Returns how many bytes it consumed from the front; the rest is offered again together
        // with the next input.
        virtual auto on_receive(std::span<std::uint8_t> input, std::size_t received) -> std::size_t = 0;

        // The connection failed or the peer closed it. No further calls follow.
        virtual void on_close(const std::error_code &error) = 0;
    };

    // A TCP connection on io(). Input is read continuously and handed to the handler; output is
    // queued and written the next time the event loop runs, so the packets of one pulse leave
    // together. Owners hold it through a shared_ptr and call close() when they are done; pending
    // handlers keep the object alive until they have run.
    class connection : public std::enable_shared_from_this<connection> {
      public:
        // max_input bounds the unconsumed input, max_output the unsent output.
        static auto create(tcp::socket socket, std::size_t max_input, std::size_t max_output) -> std::shared_ptr<connection>;

        connection(tcp::socket socket, std::size_t max_input, std::size_t max_output);

        connection(const connection &) = delete;
        auto operator=(const connection &) -> connection & = delete;

        // Starts reading; the handler must outlive the connection or call close() first.
        void start(connection_handler &handler);

        // Closes the socket at once; unsent output is dropped. The handler is not called again.
        void close();

        [[nodiscard]] auto is_open() const -> bool { return socket_.is_open(); }

        // Queues data for sending. Returns false, queuing nothing, when the connection is closed
        // or the unsent output would exceed the limit.
        auto send(std::span<const std::uint8_t> data) -> bool;
        auto send(const void *data, std::size_t size) -> bool;

        // Appends `size` bytes to the output and returns them to be filled in before control
        // returns to the event loop. Empty under the same conditions where send() fails.
        auto prepare(std::size_t size) -> std::span<std::uint8_t>;

        // Raises the output limit for good, for a peer that is sent unusually large data.
        void raise_output_limit(std::size_t size) { max_output_ += size; }

        [[nodiscard]] auto pending_output() const -> std::size_t { return output_.size() - output_sent_; }

        // Writes as much pending output as the socket takes now, without waiting for the event
        // loop; the rest follows as usual. For data that must not share a segment with what is
        // queued after it.
        void write_now() { write(); }

        // Writes all pending output, blocking for at most `timeout`. Used at shutdown.
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
        bool waiting_writable_{false};
    };
}
