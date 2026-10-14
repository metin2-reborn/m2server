#pragma once

#include <cstdint>
#include <variant>
#include <vector>

// The attribute flags (block, water, no-PvP, ...) of one sectree's cells. Cells are stored in
// the narrowest type that holds every value; a sectree whose cells all match stores none.
class CAttribute {
  public:
    CAttribute(const uint32_t *attr, uint32_t width, uint32_t height);

    void Set(uint32_t x, uint32_t y, uint32_t attr);
    void Remove(uint32_t x, uint32_t y, uint32_t attr);
    auto Get(uint32_t x, uint32_t y) const -> uint32_t;
    void CopyRow(uint32_t y, uint32_t *row) const;

  private:
    // Gives a uniform sectree its own cells before one of them changes.
    void Expand();

    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_uniform = 0; // the value of every cell while m_cells is empty
    std::variant<std::vector<uint8_t>, std::vector<uint16_t>, std::vector<uint32_t>> m_cells;
};
