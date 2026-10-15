#include "check.h"

#include <metin2/game/protocol/cipher.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

using namespace metin2::game;

namespace {
    // A client and a server cipher after the key agreement.
    struct cipher_pair {
        cipher client;
        cipher server;
        bool agreed = false;

        cipher_pair() {
            uint8_t client_key[cipher::key_size];
            uint8_t server_key[cipher::key_size];
            size_t client_length = sizeof(client_key);
            size_t server_length = sizeof(server_key);
            const size_t client_agreed = client.prepare(client_key, &client_length);
            const size_t server_agreed = server.prepare(server_key, &server_length);

            agreed = client.activate(true, client_agreed, server_key, server_length) &&
                     server.activate(false, server_agreed, client_key, client_length);
            client.set_activated(true);
            server.set_activated(true);
        }
    };

    auto pattern(size_t size) -> std::vector<uint8_t> {
        std::vector<uint8_t> data(size);
        for (size_t i = 0; i < size; ++i) {
            data[i] = static_cast<uint8_t>(i * 7 + 3);
        }
        return data;
    }
}

TEST(cipher_round_trips_both_directions) {
    cipher_pair pair;
    CHECK(pair.agreed);

    const std::vector<uint8_t> plain = pattern(300);

    std::vector<uint8_t> up = plain;
    pair.client.encrypt(up.data(), up.size());
    CHECK(up != plain);
    pair.server.decrypt(up.data(), up.size());
    CHECK(up == plain);

    std::vector<uint8_t> down = plain;
    pair.server.encrypt(down.data(), down.size());
    pair.client.decrypt(down.data(), down.size());
    CHECK(down == plain);
}

TEST(cipher_stream_continues_across_chunks) {
    cipher_pair pair;
    CHECK(pair.agreed);

    // The sender encrypts in one call, the receiver decrypts in odd-sized pieces, as TCP may
    // deliver them.
    const std::vector<uint8_t> plain = pattern(1000);
    std::vector<uint8_t> data = plain;
    pair.client.encrypt(data.data(), data.size());

    for (size_t offset = 0, step = 1; offset < data.size(); offset += step, step = step * 3 % 97 + 1) {
        const size_t size = std::min(step, data.size() - offset);
        pair.server.decrypt(data.data() + offset, size);
    }
    CHECK(data == plain);
}

TEST(cipher_rejects_bad_peer_keys) {
    cipher cipher;
    uint8_t key[metin2::game::protocol::cipher::key_size];
    size_t length = sizeof(key);
    const size_t agreed = cipher.prepare(key, &length);
    CHECK_EQ(agreed, metin2::game::protocol::cipher::key_size);

    // An all-zero key is a low-order point and gives an all-zero secret.
    const uint8_t zeros[metin2::game::protocol::cipher::key_size] = {};
    CHECK(!cipher.activate(false, agreed, zeros, sizeof(zeros)));

    // Wrong lengths, and activation without a prepared key.
    metin2::game::protocol::cipher other;
    CHECK(!other.activate(false, metin2::game::protocol::cipher::key_size, key, sizeof(key)));
    uint8_t short_buffer[8];
    size_t short_length = sizeof(short_buffer);
    CHECK_EQ(other.prepare(short_buffer, &short_length), size_t{0});
}

TEST(cipher_is_inactive_until_switched_on) {
    cipher cipher;
    std::vector<uint8_t> data = pattern(16);
    const std::vector<uint8_t> copy = data;
    cipher.encrypt(data.data(), data.size());
    CHECK(data == copy);
    CHECK(!cipher.activated());
}
