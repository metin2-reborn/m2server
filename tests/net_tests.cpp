#include "check.h"

#include <metin2/net/connection.h>
#include <metin2/net/io.h>
#include <metin2/net/listener.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <system_error>
#include <vector>

using namespace metin2;

namespace {
    using namespace std::chrono_literals;

    // Collects input, consuming it in records of `record` bytes, as packet handlers do.
    struct recorder : net::connection_handler {
        std::size_t record;
        std::vector<std::uint8_t> received;
        bool closed = false;

        explicit recorder(std::size_t record_size) : record(record_size) {}

        auto on_receive(std::span<std::uint8_t> input, std::size_t) -> std::size_t override {
            const std::size_t whole = input.size() / record * record;
            received.insert(received.end(), input.begin(), input.begin() + static_cast<std::ptrdiff_t>(whole));
            return whole;
        }

        void on_close(const std::error_code &) override { closed = true; }
    };

    template <typename Predicate>
    auto run_until(Predicate done) -> bool {
        const auto deadline = std::chrono::steady_clock::now() + 5s;
        while (!done()) {
            if (std::chrono::steady_clock::now() > deadline) {
                return false;
            }
            net::io().run_for(10ms);
            net::io().restart();
        }
        return true;
    }

    // A connected pair: `client` from net::connect, `server` from the listener.
    struct loopback {
        std::shared_ptr<net::listener> listener;
        std::optional<net::tcp::socket> accepted;
        std::optional<net::tcp::socket> client;

        loopback() {
            listener = net::listener::open("127.0.0.1", 0, [this](net::tcp::socket socket) { accepted.emplace(std::move(socket)); });
            if (listener) {
                client = net::connect("127.0.0.1", listener->port(), 2s);
                run_until([this] { return accepted.has_value(); });
            }
        }

        ~loopback() {
            if (listener) {
                listener->close();
            }
        }
    };
}

TEST(listener_picks_a_port) {
    loopback pair;
    CHECK(pair.listener != nullptr);
    CHECK(pair.listener && pair.listener->port() != 0);
    CHECK(pair.client.has_value());
    CHECK(pair.accepted.has_value());
}

TEST(connection_delivers_data_in_order) {
    loopback pair;
    CHECK(pair.client && pair.accepted);
    if (!pair.client || !pair.accepted) {
        return;
    }

    recorder server_side(4);
    auto server = net::connection::create(std::move(*pair.accepted), 1 << 16, 1 << 16);
    server->start(server_side);

    recorder client_side(1);
    auto client = net::connection::create(std::move(*pair.client), 1 << 16, 1 << 16);
    client->start(client_side);

    // Ten bytes: the handler takes whole 4-byte records and leaves the rest for the next read.
    std::vector<std::uint8_t> data(10);
    for (std::size_t i = 0; i < data.size(); ++i) {
        data[i] = static_cast<std::uint8_t>(i + 1);
    }
    CHECK(client->send(data.data(), data.size()));
    CHECK(run_until([&] { return server_side.received.size() == 8; }));

    // Two more bytes complete the third record.
    const std::uint8_t tail[2] = {11, 12};
    CHECK(client->send(tail, sizeof(tail)));
    CHECK(run_until([&] { return server_side.received.size() == 12; }));
    for (std::size_t i = 0; i < server_side.received.size(); ++i) {
        CHECK_EQ(server_side.received[i], static_cast<std::uint8_t>(i + 1));
    }

    // And back the other way.
    const std::uint8_t reply[3] = {'a', 'b', 'c'};
    CHECK(server->send(reply, sizeof(reply)));
    CHECK(run_until([&] { return client_side.received.size() == 3; }));

    client->close();
    server->close();
}

TEST(connection_reports_peer_close) {
    loopback pair;
    if (!pair.client || !pair.accepted) {
        CHECK(false);
        return;
    }

    recorder server_side(1);
    auto server = net::connection::create(std::move(*pair.accepted), 1 << 16, 1 << 16);
    server->start(server_side);

    pair.client->close();
    CHECK(run_until([&] { return server_side.closed; }));
    CHECK(!server->is_open());
}

TEST(connection_enforces_output_limit) {
    loopback pair;
    if (!pair.client || !pair.accepted) {
        CHECK(false);
        return;
    }

    recorder side(1);
    auto connection = net::connection::create(std::move(*pair.client), 1 << 16, 100);
    connection->start(side);

    std::vector<std::uint8_t> data(60);
    CHECK(connection->send(data.data(), data.size()));
    CHECK(!connection->send(data.data(), data.size())); // 120 bytes would pass the limit
    CHECK(connection->prepare(50).empty());
    connection->close();
    CHECK(!connection->send(data.data(), 1));
}

TEST(connect_fails_on_closed_port) {
    // Bind a port and close it again, so nothing listens there.
    std::uint16_t port = 0;
    {
        auto listener = net::listener::open("127.0.0.1", 0, [](net::tcp::socket) {});
        CHECK(listener != nullptr);
        if (listener) {
            port = listener->port();
            listener->close();
        }
    }
    net::io().poll();
    net::io().restart();
    CHECK(!net::connect("127.0.0.1", port, 1s).has_value());
}
