#pragma once

#include <vector>

namespace metin2::inline common {

// Occupancy of a width x height cell grid (inventory pages, shops, exchange windows).
// Positions are row-major cell indices; an item covers w x h cells from its top-left cell.
class CGrid {
  public:
    CGrid(int w, int h);
    // Copy of other with new dimensions: the first cells carry over, any new ones start empty.
    CGrid(const CGrid &other, int w, int h);

    void Clear();
    auto FindBlank(int w, int h) const -> int;
    auto IsEmpty(int iPos, int w, int h) const -> bool;
    auto Put(int iPos, int w, int h) -> bool;
    void Get(int iPos, int w, int h);
    auto GetSize() const -> unsigned int;

  private:
    void Fill(int iPos, int w, int h, char value);

    int m_iWidth;
    int m_iHeight;
    std::vector<char> m_cells;
};
}
