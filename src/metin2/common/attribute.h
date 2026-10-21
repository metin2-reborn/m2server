#pragma once

#include <cstdint>
#include <variant>
#include <vector>

namespace metin2::inline common {
    class attribute {
      public:
        attribute(const uint32_t *attr, uint32_t width, uint32_t height);

        void set(uint32_t x, uint32_t y, uint32_t attr);
        void remove(uint32_t x, uint32_t y, uint32_t attr);
        auto get(uint32_t x, uint32_t y) const -> uint32_t;
        void copy_row(uint32_t y, uint32_t *row) const;

      private:
        void expand();

        uint32_t width_;
        uint32_t height_;
        uint32_t uniform_ = 0;
        std::variant<std::vector<uint8_t>, std::vector<uint16_t>, std::vector<uint32_t>> cells_;
    };
}
