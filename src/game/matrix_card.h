#ifndef __INC_DB_MATRIXCARD_H__
#define __INC_DB_MATRIXCARD_H__

#include <cstdint>

extern void MatrixCardRndCoordinate(uint32_t & rows, uint32_t & cols);

// The i-th of the four coordinates packed into rows/cols, most significant byte first.
constexpr uint32_t MATRIX_CARD_ROW(uint32_t rows, int i) { return (rows >> ((4 - i - 1) * 8)) & 0x000000FF; }
constexpr uint32_t MATRIX_CARD_COL(uint32_t cols, int i) { return (cols >> ((4 - i - 1) * 8)) & 0x000000FF; }

#endif
