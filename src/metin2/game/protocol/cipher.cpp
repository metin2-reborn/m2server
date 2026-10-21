
#include <metin2/game/protocol/cipher.h>

#include <monocypher.h>

#include <cstdint>
#include <cstring>

#include <sys/random.h>
#include <sys/types.h>

namespace metin2::game::inline protocol {

    namespace {
        const uint8_t nonce[8] = {};
        const char label[] = "metin2 connection keys v1";

        auto fill_random(uint8_t *buffer, size_t size) -> bool {
            while (size > 0) {
                ssize_t n = getrandom(buffer, size, 0);
                if (n <= 0)
                    return false;
                buffer += n;
                size -= static_cast<size_t>(n);
            }
            return true;
        }
    }

    void cipher::stream::reset(const uint8_t *new_key) {
        memcpy(key, new_key, key_size);
        counter = 0;
        used = sizeof(block);
    }

    void cipher::stream::wipe() {
        crypto_wipe(key, sizeof(key));
        crypto_wipe(block, sizeof(block));
        counter = 0;
        used = sizeof(block);
    }

    void cipher::stream::process(uint8_t *data, size_t length) {
        while (length > 0 && used < sizeof(block)) {
            *data++ ^= block[used++];
            --length;
        }

        const size_t whole = length - length % sizeof(block);
        if (whole > 0) {
            counter = crypto_chacha20_djb(data, data, whole, key, nonce, counter);
            data += whole;
            length -= whole;
        }

        if (length > 0) {
            static const uint8_t zeros[64] = {};
            counter = crypto_chacha20_djb(block, zeros, sizeof(block), key, nonce, counter);
            used = 0;
            while (length > 0) {
                *data++ ^= block[used++];
                --length;
            }
        }
    }

    cipher::cipher() = default;

    cipher::~cipher() {
        clean_up();
    }

    void cipher::clean_up() {
        crypto_wipe(secret_key_, sizeof(secret_key_));
        crypto_wipe(public_key_, sizeof(public_key_));
        encoder_.wipe();
        decoder_.wipe();
        key_prepared_ = false;
        activated_ = false;
    }

    auto cipher::prepare(void *buffer, size_t *length) -> size_t {
        if (*length < key_size || !fill_random(secret_key_, sizeof(secret_key_)))
            return 0;

        crypto_x25519_public_key(public_key_, secret_key_);
        memcpy(buffer, public_key_, key_size);
        *length = key_size;
        key_prepared_ = true;
        return key_size;
    }

    auto cipher::activate(bool polarity, size_t agreed_length, const void *buffer, size_t length) -> bool {
        if (!key_prepared_ || agreed_length != key_size || length != key_size)
            return false;
        key_prepared_ = false;

        const auto *peer_key = static_cast<const uint8_t *>(buffer);
        uint8_t shared[key_size];
        crypto_x25519(shared, secret_key_, peer_key);
        crypto_wipe(secret_key_, sizeof(secret_key_));

        static const uint8_t zeros[key_size] = {};
        if (crypto_verify32(shared, zeros) == 0) {
            crypto_wipe(shared, sizeof(shared));
            return false;
        }

        // keys = BLAKE2b-512(key = shared, label || client public key || server public key)
        const uint8_t *client_key = polarity ? public_key_ : peer_key;
        const uint8_t *server_key = polarity ? peer_key : public_key_;
        uint8_t message[sizeof(label) - 1 + 2 * key_size];
        memcpy(message, label, sizeof(label) - 1);
        memcpy(message + sizeof(label) - 1, client_key, key_size);
        memcpy(message + sizeof(label) - 1 + key_size, server_key, key_size);

        uint8_t keys[2 * key_size];
        crypto_blake2b_keyed(keys, sizeof(keys), shared, sizeof(shared), message, sizeof(message));
        crypto_wipe(shared, sizeof(shared));

        const uint8_t *client_to_server = keys;
        const uint8_t *server_to_client = keys + key_size;
        encoder_.reset(polarity ? client_to_server : server_to_client);
        decoder_.reset(polarity ? server_to_client : client_to_server);
        crypto_wipe(keys, sizeof(keys));
        return true;
    }
}
