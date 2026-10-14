#include <libnet/listener.h>

#include <libnet/io.h>

#include <libthecore/log.h>

#include <asio/ip/address.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/socket_base.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <system_error>
#include <utility>

namespace net {
    auto listener::open(const std::string &address, std::uint16_t port, accept_handler on_accept) -> std::shared_ptr<listener> {
        std::error_code error;
        const auto ip = asio::ip::make_address(address, error);
        if (error) {
            sys_err("invalid listen address %s: %s", address.c_str(), error.message().c_str());
            return nullptr;
        }

        const tcp::endpoint endpoint(ip, port);
        tcp::acceptor acceptor(io());
        acceptor.open(endpoint.protocol(), error);
        if (!error) acceptor.set_option(tcp::acceptor::reuse_address(true), error);
        if (!error) acceptor.bind(endpoint, error);
        if (!error) acceptor.listen(asio::socket_base::max_listen_connections, error);
        if (error) {
            sys_err("cannot listen on %s:%u: %s", address.c_str(), port, error.message().c_str());
            return nullptr;
        }

        auto result = std::make_shared<listener>(std::move(acceptor), std::move(on_accept));
        result->accept();
        return result;
    }

    listener::listener(tcp::acceptor acceptor, accept_handler on_accept)
        : acceptor_(std::move(acceptor)), on_accept_(std::move(on_accept)) {
    }

    void listener::close() {
        std::error_code error;
        acceptor_.close(error);
    }

    void listener::accept() {
        acceptor_.async_accept([self = shared_from_this()](const std::error_code &error, tcp::socket socket) {
            if (!self->acceptor_.is_open()) {
                return;
            }
            if (!error) {
                self->on_accept_(std::move(socket));
            } else {
                sys_err("accept: %s", error.message().c_str());
            }
            self->accept();
        });
    }
}
