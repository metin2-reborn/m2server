#include <metin2/common/grid.h>

#include <algorithm>
#include <cstddef>

namespace metin2::inline common {
    grid::grid(int w, int h) : width_(w), height_(h), cells_(static_cast<std::size_t>(w) * h) {}

    grid::grid(const grid &other, int w, int h) : grid(w, h) {
        const std::size_t count = std::min(cells_.size(), other.cells_.size());
        std::copy_n(other.cells_.begin(), count, cells_.begin());
    }

    void grid::clear() {
        std::ranges::fill(cells_, 0);
    }

    auto grid::find_blank(int w, int h) const -> int {
        if (w > width_ || h > height_) {
            return -1;
        }

        for (int iIndex = 0; iIndex < width_ * height_; ++iIndex) {
            if (is_empty(iIndex, w, h)) {
                return iIndex;
            }
        }

        return -1;
    }

    auto grid::put(int pos, int w, int h) -> bool {
        if (!is_empty(pos, w, h)) {
            return false;
        }

        fill(pos, w, h, 1);
        return true;
    }

    void grid::get(int pos, int w, int h) {
        if (pos < 0 || pos >= width_ * height_) {
            return;
        }

        fill(pos, w, h, 0);
    }

    auto grid::size() const -> unsigned int {
        return static_cast<unsigned int>(cells_.size());
    }

    auto grid::is_empty(int pos, int w, int h) const -> bool {
        if (pos < 0) {
            return false;
        }

        const int iRow = pos / width_;
        if (iRow + h > height_ || pos % width_ + w > width_) {
            return false;
        }

        for (int y = 0; y < h; ++y) {
            const auto row = cells_.begin() + pos + y * width_;
            if (std::any_of(row, row + w, [](char cell) -> bool { return cell != 0; })) {
                return false;
            }
        }

        return true;
    }

    void grid::fill(int pos, int w, int h, char value) {
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const std::size_t index = pos + y * width_ + x;
                if (index < cells_.size()) {
                    cells_[index] = value;
                }
            }
        }
    }
}
