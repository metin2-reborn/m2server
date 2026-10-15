#include <metin2/net/connection.h>

#include <metin2/net/io.h>

#include <metin2/log/log.h>

#include <asio/buffer.hpp>
#include <asio/error.hpp>
#include <asio/post.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <system_error>
#include <utility>
#include <vector>

namespace metin2::net {
    namespace {
        // Input grows in steps of at least this much free space per read.
        constexpr std::size_t min_read_space = 16 * 1024;
        // Buffers grown beyond this by a burst are released once they are empty again.
        constexpr std::size_t keep_capacity = 1024 * 1024;

        void release(std::vector<std::uint8_t> &output, std::size_t &sent) {
            output.clear();
            sent = 0;
            if (output.capacity() > keep_capacity) {
                output.shrink_to_fit();
            }
        }
    }

    auto connection::create(tcp::socket socket, std::size_t max_input, std::size_t max_output) -> std::shared_ptr<connection> {
        return std::make_shared<connection>(std::move(socket), max_input, max_output);
    }

    connection::connection(tcp::socket socket, std::size_t max_input, std::size_t max_output)
        : socket_(std::move(socket)), max_input_(max_input), max_output_(max_output) {
        std::error_code error;
        const auto endpoint = socket_.remote_endpoint(error);
        if (!error) {
            address_ = endpoint.address().to_string();
            port_ = endpoint.port();
        }
        socket_.non_blocking(true, error);
    }

    void connection::start(connection_handler &handler) {
        handler_ = &handler;
        read();
    }

    void connection::close() {
        handler_ = nullptr;
        std::error_code error;
        socket_.close(error);
        release(output_, output_sent_);
    }

    void connection::fail(const std::error_code &error) {
        connection_handler *handler = std::exchange(handler_, nullptr);
        close();
        if (handler) {
            handler->on_close(error);
        }
    }

    void connection::read() {
        if (input_.size() - input_size_ < min_read_space) {
            if (input_size_ >= max_input_) {
                metin2::log::error("{}: input exceeds {:d} bytes", address_.c_str(), max_input_);
                fail(asio::error::message_size);
                return;
            }
            input_.resize(std::min(max_input_, std::max(input_.size() * 2, input_size_ + min_read_space)));
        }

        socket_.async_read_some(asio::buffer(input_.data() + input_size_, input_.size() - input_size_),
            [self = shared_from_this()](const std::error_code &error, std::size_t size) { self->on_read(error, size); });
    }

    void connection::on_read(const std::error_code &error, std::size_t size) {
        if (!handler_) {
            return; // closed
        }
        if (error) {
            fail(error);
            return;
        }

        input_size_ += size;
        const std::size_t consumed = handler_->on_receive(std::span(input_.data(), input_size_), size);
        if (!handler_) {
            return; // the handler closed the connection
        }
        if (consumed > input_size_) {
            // never move or drop more than was received
            metin2::log::error("{}: handler consumed {:d} of {:d} bytes", address_.c_str(), consumed, input_size_);
            fail(asio::error::invalid_argument);
            return;
        }

        if (consumed > 0) {
            std::memmove(input_.data(), input_.data() + consumed, input_size_ - consumed);
            input_size_ -= consumed;
        }
        if (input_size_ == 0 && input_.size() > keep_capacity) {
            input_.clear();
            input_.shrink_to_fit();
        }
        read();
    }

    auto connection::send(std::span<const std::uint8_t> data) -> bool {
        const auto space = prepare(data.size());
        if (space.size() != data.size()) {
            return false;
        }
        std::memcpy(space.data(), data.data(), data.size());
        return true;
    }

    auto connection::send(const void *data, std::size_t size) -> bool {
        return send(std::span(static_cast<const std::uint8_t *>(data), size));
    }

    auto connection::prepare(std::size_t size) -> std::span<std::uint8_t> {
        if (!socket_.is_open() || pending_output() + size > max_output_) {
            return {};
        }

        const std::size_t offset = output_.size();
        output_.resize(offset + size);
        schedule_write();
        return std::span(output_.data() + offset, size);
    }

    void connection::schedule_write() {
        if (write_scheduled_ || waiting_writable_) {
            return;
        }
        write_scheduled_ = true;
        asio::post(socket_.get_executor(), [self = shared_from_this()] {
            self->write_scheduled_ = false;
            self->write();
        });
    }

    void connection::write() {
        while (socket_.is_open() && pending_output() > 0) {
            std::error_code error;
            const std::size_t size = socket_.write_some(asio::buffer(output_.data() + output_sent_, pending_output()), error);

            if (error == asio::error::would_block) {
                waiting_writable_ = true;
                socket_.async_wait(tcp::socket::wait_write, [self = shared_from_this()](const std::error_code &wait_error) {
                    self->waiting_writable_ = false;
                    if (!wait_error) self->write();
                });
                return;
            }
            if (error) {
                fail(error);
                return;
            }

            output_sent_ += size;
            detail::count_written(size);
        }

        release(output_, output_sent_);
    }

    auto connection::flush(std::chrono::milliseconds timeout) -> bool {
        if (!socket_.is_open()) {
            return false;
        }
        if (!net::write(socket_, std::span(output_.data() + output_sent_, pending_output()), timeout)) {
            return false;
        }
        release(output_, output_sent_);
        return true;
    }
}
