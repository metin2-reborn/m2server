#pragma once

#include <vector>

namespace metin2::inline common {

// Occupancy of a width x height cell grid (inventory pages, shops, exchange windows).
// Positions are row-major cell indices; an item covers w x h cells from its top-left cell.
class grid {
  public:
    grid(int w, int h);
    // Copy of other with new dimensions: the first cells carry over, any new ones start empty.
    grid(const grid &other, int w, int h);

    void clear();
    auto find_blank(int w, int h) const -> int;
    auto is_empty(int pos, int w, int h) const -> bool;
    auto put(int pos, int w, int h) -> bool;
    void get(int pos, int w, int h);
    auto size() const -> unsigned int;

  private:
    void fill(int pos, int w, int h, char value);

    int width_;
    int height_;
    std::vector<char> cells_;
};
}
