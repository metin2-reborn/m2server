#pragma once

#include <array>
#include <cstdint>

namespace metin2::game::inline protocol {

    struct packet_spec {
        int size = 0;
        const char *name = nullptr;
        bool sequence = false;
    };

    // Indexed by header; entries without a name are unknown headers. A sequenced packet's size includes
    // its trailing sequence byte.
    class packet_table {
      public:
        auto find(uint8_t header) const -> const packet_spec * {
            const packet_spec &spec = specs_[header];
            return spec.name ? &spec : nullptr;
        }

        auto is_sequence(uint8_t header) const -> bool {
            const packet_spec *spec = find(header);
            return spec && spec->sequence;
        }

        void set(uint8_t header, int size, const char *name, bool sequence);
        void set_sequence(uint8_t header, bool sequence);

      private:
        std::array<packet_spec, 256> specs_{};
    };

    auto client_packet_table() -> const packet_table &;
    // The client table as seen before login: the handshake's PONG carries no sequence byte.
    auto handshake_packet_table() -> const packet_table &;
    auto p2p_packet_table() -> const packet_table &;

}
