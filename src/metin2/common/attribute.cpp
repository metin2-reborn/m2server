#include <metin2/common/attribute.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <numeric>
#include <variant>
#include <vector>

namespace metin2::inline common {

CAttribute::CAttribute(const uint32_t *attr, uint32_t width, uint32_t height) : m_width(width), m_height(height) {
    const std::size_t size = static_cast<std::size_t>(width) * height;
    if (size == 0) {
        return;
    }

    if (std::all_of(attr, attr + size, [attr](uint32_t value) -> bool { return value == attr[0]; })) {
        m_uniform = attr[0];
        return;
    }

    const uint32_t used = std::accumulate(attr, attr + size, uint32_t{0}, std::bit_or<>{});
    if ((used & 0xffffff00) == 0) {
        m_cells = std::vector<uint8_t>(attr, attr + size);
    } else if ((used & 0xffff0000) == 0) {
        m_cells = std::vector<uint16_t>(attr, attr + size);
    } else {
        m_cells = std::vector<uint32_t>(attr, attr + size);
    }
}

void CAttribute::Expand() {
    std::visit(
        [this](auto &cells) -> void {
            if (cells.empty()) {
                m_cells = std::vector<uint32_t>(static_cast<std::size_t>(m_width) * m_height, m_uniform);
            }
        },
        m_cells);
}

void CAttribute::Set(uint32_t x, uint32_t y, uint32_t attr) {
    if (x >= m_width || y >= m_height) {
        return;
    }

    Expand();
    std::visit([&](auto &cells) -> void { cells[y * m_width + x] |= attr; }, m_cells);
}

void CAttribute::Remove(uint32_t x, uint32_t y, uint32_t attr) {
    if (x >= m_width || y >= m_height) {
        return;
    }

    if (std::visit([](const auto &cells) -> bool { return cells.empty(); }, m_cells) && (m_uniform & attr) == 0) {
        return;
    }

    Expand();
    std::visit([&](auto &cells) -> void { cells[y * m_width + x] &= ~attr; }, m_cells);
}

auto CAttribute::Get(uint32_t x, uint32_t y) const -> uint32_t {
    if (x >= m_width || y >= m_height) {
        return 0;
    }

    return std::visit([&](const auto &cells) -> uint32_t { return cells.empty() ? m_uniform : cells[y * m_width + x]; }, m_cells);
}

void CAttribute::CopyRow(uint32_t y, uint32_t *row) const {
    std::visit(
        [&](const auto &cells) -> void {
            if (cells.empty()) {
                std::fill_n(row, m_width, m_uniform);
            } else {
                std::copy_n(cells.begin() + static_cast<std::size_t>(y) * m_width, m_width, row);
            }
        },
        m_cells);
}
}
