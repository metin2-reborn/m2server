#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace metin2::game::inline protocol {

class temp_buffer {
  public:
    explicit temp_buffer(std::size_t reserve = 8192) { data_.reserve(reserve); }

    auto read_peek() const -> const void * { return data_.data(); }
    void write(const void *data, std::size_t size) {
        const auto *bytes = static_cast<const uint8_t *>(data);
        data_.insert(data_.end(), bytes, bytes + size);
    }
    auto size() const -> int { return static_cast<int>(data_.size()); }

  private:
    std::vector<uint8_t> data_;
};
}
