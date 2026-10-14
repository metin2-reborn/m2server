
#include "cipher.h"

#include <monocypher.h>

#include <cstdint>
#include <cstring>

#include <sys/random.h>
#include <sys/types.h>

namespace {
    const uint8_t kNonce[8] = {};
    const char kLabel[] = "metin2 connection keys v1";

    auto FillRandom(uint8_t *buffer, size_t size) -> bool {
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

void Cipher::Stream::Reset(const uint8_t *new_key) {
    memcpy(key, new_key, kKeySize);
    counter = 0;
    used = sizeof(block);
}

void Cipher::Stream::Wipe() {
    crypto_wipe(key, sizeof(key));
    crypto_wipe(block, sizeof(block));
    counter = 0;
    used = sizeof(block);
}

void Cipher::Stream::Process(uint8_t *data, size_t length) {
    // the rest of the current keystream block
    while (length > 0 && used < sizeof(block)) {
        *data++ ^= block[used++];
        --length;
    }

    // whole blocks in place
    const size_t whole = length - length % sizeof(block);
    if (whole > 0) {
        counter = crypto_chacha20_djb(data, data, whole, key, kNonce, counter);
        data += whole;
        length -= whole;
    }

    // start a new block for the tail
    if (length > 0) {
        static const uint8_t zeros[64] = {};
        counter = crypto_chacha20_djb(block, zeros, sizeof(block), key, kNonce, counter);
        used = 0;
        while (length > 0) {
            *data++ ^= block[used++];
            --length;
        }
    }
}

Cipher::Cipher() = default;

Cipher::~Cipher() {
    CleanUp();
}

void Cipher::CleanUp() {
    crypto_wipe(secret_key_, sizeof(secret_key_));
    crypto_wipe(public_key_, sizeof(public_key_));
    encoder_.Wipe();
    decoder_.Wipe();
    key_prepared_ = false;
    activated_ = false;
}

auto Cipher::Prepare(void *buffer, size_t *length) -> size_t {
    if (*length < kKeySize || !FillRandom(secret_key_, sizeof(secret_key_)))
        return 0;

    crypto_x25519_public_key(public_key_, secret_key_);
    memcpy(buffer, public_key_, kKeySize);
    *length = kKeySize;
    key_prepared_ = true;
    return kKeySize;
}

auto Cipher::Activate(bool polarity, size_t agreed_length, const void *buffer, size_t length) -> bool {
    if (!key_prepared_ || agreed_length != kKeySize || length != kKeySize)
        return false;
    key_prepared_ = false;

    const auto *peer_key = static_cast<const uint8_t *>(buffer);
    uint8_t shared[kKeySize];
    crypto_x25519(shared, secret_key_, peer_key);
    crypto_wipe(secret_key_, sizeof(secret_key_));

    // a low-order peer key yields an all-zero secret
    static const uint8_t zeros[kKeySize] = {};
    if (crypto_verify32(shared, zeros) == 0) {
        crypto_wipe(shared, sizeof(shared));
        return false;
    }

    // keys = BLAKE2b-512(key = shared, label || client public key || server public key)
    const uint8_t *client_key = polarity ? public_key_ : peer_key;
    const uint8_t *server_key = polarity ? peer_key : public_key_;
    uint8_t message[sizeof(kLabel) - 1 + 2 * kKeySize];
    memcpy(message, kLabel, sizeof(kLabel) - 1);
    memcpy(message + sizeof(kLabel) - 1, client_key, kKeySize);
    memcpy(message + sizeof(kLabel) - 1 + kKeySize, server_key, kKeySize);

    uint8_t keys[2 * kKeySize];
    crypto_blake2b_keyed(keys, sizeof(keys), shared, sizeof(shared), message, sizeof(message));
    crypto_wipe(shared, sizeof(shared));

    const uint8_t *client_to_server = keys;
    const uint8_t *server_to_client = keys + kKeySize;
    encoder_.Reset(polarity ? client_to_server : server_to_client);
    decoder_.Reset(polarity ? server_to_client : client_to_server);
    crypto_wipe(keys, sizeof(keys));
    return true;
}

