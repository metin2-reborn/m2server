#pragma once

#include <cstddef>
#include <cstdint>

namespace metin2::game {

// Encryption of the game connection (the client's EterBase/Cipher is the same code):
//   - key agreement: X25519 with a fresh key pair per connection; each side sends its 32-byte
//   - keys: BLAKE2b-512 keyed with the shared secret over a label and both public keys (client's
//     first); the first half encrypts client -> server, the second half server -> client;
//   - stream: ChaCha20 per direction (nonce 0, the keys are unique per connection), applied in
//     place, so packets keep their sizes.
// Like the protocol before it, there is no message authentication and no server authentication.
class Cipher {
  public:
    static constexpr size_t kKeySize = 32;

    Cipher();
    ~Cipher();

    void CleanUp();

    // Writes our public key into buffer (*length: in capacity, out size); returns the agreed value
    // length, or 0 on failure.
    auto Prepare(void *buffer, size_t *length) -> size_t;

    // Agrees on the keys with the peer's public key. polarity is true on the client.
    auto Activate(bool polarity, size_t agreed_length, const void *buffer, size_t length) -> bool;

    void Encrypt(void *buffer, size_t length) {
        if (activated_)
            encoder_.Process(static_cast<uint8_t *>(buffer), length);
    }

    void Decrypt(void *buffer, size_t length) {
        if (activated_)
            decoder_.Process(static_cast<uint8_t *>(buffer), length);
    }

    [[nodiscard]] auto activated() const -> bool { return activated_; }

    // The protocol switches encryption on at a defined point after Activate.
    void set_activated(bool value) { activated_ = value; }

    [[nodiscard]] auto IsKeyPrepared() const -> bool { return key_prepared_; }

  private:
    // ChaCha20 keystream that continues across calls.
    struct Stream {
        uint8_t key[kKeySize] = {};
        uint64_t counter = 0;
        uint8_t block[64] = {};
        size_t used = 64; // bytes of block already consumed

        void Reset(const uint8_t *new_key);
        void Wipe();
        void Process(uint8_t *data, size_t length);
    };

    bool activated_ = false;
    bool key_prepared_ = false;
    uint8_t secret_key_[kKeySize] = {};
    uint8_t public_key_[kKeySize] = {};
    Stream encoder_;
    Stream decoder_;
};
}
