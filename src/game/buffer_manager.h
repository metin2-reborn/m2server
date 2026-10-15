#ifndef __INC_METIN_II_GAME_BUFFER_MANAGER_H__
#define __INC_METIN_II_GAME_BUFFER_MANAGER_H__

#include <cstddef>
#include <cstdint>
#include <vector>

// Scratch space for assembling a packet from several pieces before sending it in one call.
class TEMP_BUFFER {
  public:
    explicit TEMP_BUFFER(std::size_t reserve = 8192) { m_data.reserve(reserve); }

    auto read_peek() const -> const void * { return m_data.data(); }
    void write(const void *data, std::size_t size) {
        const auto *bytes = static_cast<const uint8_t *>(data);
        m_data.insert(m_data.end(), bytes, bytes + size);
    }
    auto size() const -> int { return static_cast<int>(m_data.size()); }

  private:
    std::vector<uint8_t> m_data;
};

#endif
