#pragma once

#include <cstddef>
#include <cstdint>

namespace metin2::game::inline protocol {

    // Same as the client cipher: X25519 per connection; BLAKE2b-512 keyed with the shared secret
    // over a label and both public keys (client first), first half client -> server, second half
    // server -> client; ChaCha20 per direction in place, so packet sizes do not change. There is no
    // message or server authentication.
    class cipher {
      public:
        static constexpr size_t key_size = 32;

        cipher();
        ~cipher();

        void clean_up();

        auto prepare(void *buffer, size_t *length) -> size_t;

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

        void set_activated(bool value) { activated_ = value; }

        [[nodiscard]] auto is_key_prepared() const -> bool { return key_prepared_; }

      private:
        struct stream {
            uint8_t key[key_size] = {};
            uint64_t counter = 0;
            uint8_t block[64] = {};
            size_t used = 64;

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
