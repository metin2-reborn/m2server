#include <libgame/grid.h>

#include <algorithm>
#include <cstddef>

namespace metin2::inline common {

CGrid::CGrid(int w, int h) : m_iWidth(w), m_iHeight(h), m_cells(static_cast<std::size_t>(w) * h) {}

CGrid::CGrid(const CGrid &other, int w, int h) : CGrid(w, h) {
    const std::size_t count = std::min(m_cells.size(), other.m_cells.size());
    std::copy_n(other.m_cells.begin(), count, m_cells.begin());
}

void CGrid::Clear() {
    std::ranges::fill(m_cells, 0);
}

auto CGrid::FindBlank(int w, int h) const -> int {
    if (w > m_iWidth || h > m_iHeight) {
        return -1;
    }

    for (int iIndex = 0; iIndex < m_iWidth * m_iHeight; ++iIndex) {
        if (IsEmpty(iIndex, w, h)) {
            return iIndex;
        }
    }

    return -1;
}

auto CGrid::Put(int iPos, int w, int h) -> bool {
    if (!IsEmpty(iPos, w, h)) {
        return false;
    }

    Fill(iPos, w, h, 1);
    return true;
}

void CGrid::Get(int iPos, int w, int h) {
    if (iPos < 0 || iPos >= m_iWidth * m_iHeight) {
        return;
    }

    Fill(iPos, w, h, 0);
}

auto CGrid::GetSize() const -> unsigned int {
    return static_cast<unsigned int>(m_cells.size());
}

auto CGrid::IsEmpty(int iPos, int w, int h) const -> bool {
    if (iPos < 0) {
        return false;
    }

    // The block must lie inside the grid without wrapping into the next row.
    const int iRow = iPos / m_iWidth;
    if (iRow + h > m_iHeight || iPos % m_iWidth + w > m_iWidth) {
        return false;
    }

    for (int y = 0; y < h; ++y) {
        const auto row = m_cells.begin() + iPos + y * m_iWidth;
        if (std::any_of(row, row + w, [](char cell) -> bool { return cell != 0; })) {
            return false;
        }
    }

    return true;
}

void CGrid::Fill(int iPos, int w, int h, char value) {
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const std::size_t index = iPos + y * m_iWidth + x;
            if (index < m_cells.size()) {
                m_cells[index] = value;
            }
        }
    }
}
}
