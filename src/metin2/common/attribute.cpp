#include <metin2/common/attribute.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <numeric>
#include <variant>
#include <vector>

namespace metin2::inline common {

attribute::attribute(const uint32_t *attr, uint32_t width, uint32_t height) : width_(width), height_(height) {
    const std::size_t size = static_cast<std::size_t>(width) * height;
    if (size == 0) {
        return;
    }

    if (std::all_of(attr, attr + size, [attr](uint32_t value) -> bool { return value == attr[0]; })) {
        uniform_ = attr[0];
        return;
    }

    const uint32_t used = std::accumulate(attr, attr + size, uint32_t{0}, std::bit_or<>{});
    if ((used & 0xffffff00) == 0) {
        cells_ = std::vector<uint8_t>(attr, attr + size);
    } else if ((used & 0xffff0000) == 0) {
        cells_ = std::vector<uint16_t>(attr, attr + size);
    } else {
        cells_ = std::vector<uint32_t>(attr, attr + size);
    }
}

void attribute::expand() {
    std::visit(
        [this](auto &cells) -> void {
            if (cells.empty()) {
                cells_ = std::vector<uint32_t>(static_cast<std::size_t>(width_) * height_, uniform_);
            }
        },
        cells_);
}

void attribute::set(uint32_t x, uint32_t y, uint32_t attr) {
    if (x >= width_ || y >= height_) {
        return;
    }

    expand();
    std::visit([&](auto &cells) -> void { cells[y * width_ + x] |= attr; }, cells_);
}

void attribute::remove(uint32_t x, uint32_t y, uint32_t attr) {
    if (x >= width_ || y >= height_) {
        return;
    }

    if (std::visit([](const auto &cells) -> bool { return cells.empty(); }, cells_) && (uniform_ & attr) == 0) {
        return;
    }

    expand();
    std::visit([&](auto &cells) -> void { cells[y * width_ + x] &= ~attr; }, cells_);
}

auto attribute::get(uint32_t x, uint32_t y) const -> uint32_t {
    if (x >= width_ || y >= height_) {
        return 0;
    }

    return std::visit([&](const auto &cells) -> uint32_t { return cells.empty() ? uniform_ : cells[y * width_ + x]; }, cells_);
}

void attribute::copy_row(uint32_t y, uint32_t *row) const {
    std::visit(
        [&](const auto &cells) -> void {
            if (cells.empty()) {
                std::fill_n(row, width_, uniform_);
            } else {
                std::copy_n(cells.begin() + static_cast<std::size_t>(y) * width_, width_, row);
            }
        },
        cells_);
}
}
