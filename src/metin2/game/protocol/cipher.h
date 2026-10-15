#pragma once

#include <cstddef>
#include <cstdint>

namespace metin2::game::inline protocol {

// Encryption of the game connection (the client's EterBase/Cipher is the same code):
//   - key agreement: X25519 with a fresh key pair per connection; each side sends its 32-byte
//   - keys: BLAKE2b-512 keyed with the shared secret over a label and both public keys (client's
//     first); the first half encrypts client -> server, the second half server -> client;
//   - stream: ChaCha20 per direction (nonce 0, the keys are unique per connection), applied in
//     place, so packets keep their sizes.
// Like the protocol before it, there is no message authentication and no server authentication.
class cipher {
  public:
    static constexpr size_t key_size = 32;

    cipher();
    ~cipher();

    void clean_up();

    // Writes our public key into buffer (*length: in capacity, out size); returns the agreed value
    // length, or 0 on failure.
    auto prepare(void *buffer, size_t *length) -> size_t;

    // Agrees on the keys with the peer's public key. polarity is true on the client.
    auto activate(bool polarity, size_t agreed_length, const void *buffer, size_t length) -> bool;

    void encrypt(void *buffer, size_t length) {
        if (activated_)
            encoder_.process(static_cast<uint8_t *>(buffer), length);
    }

    void decrypt(void *buffer, size_t length) {
        if (activated_)
            decoder_.process(static_cast<uint8_t *>(buffer), length);
    }

    [[nodiscard]] auto activated() const -> bool { return activated_; }

    // The protocol switches encryption on at a defined point after Activate.
    void set_activated(bool value) { activated_ = value; }

    [[nodiscard]] auto is_key_prepared() const -> bool { return key_prepared_; }

  private:
    // ChaCha20 keystream that continues across calls.
    struct stream {
        uint8_t key[key_size] = {};
        uint64_t counter = 0;
        uint8_t block[64] = {};
        size_t used = 64; // bytes of block already consumed

        void reset(const uint8_t *new_key);
        void wipe();
        void process(uint8_t *data, size_t length);
    };

    bool activated_ = false;
    bool key_prepared_ = false;
    uint8_t secret_key_[key_size] = {};
    uint8_t public_key_[key_size] = {};
    stream encoder_;
    stream decoder_;
};
}
