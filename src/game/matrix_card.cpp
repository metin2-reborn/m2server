//#define __MATRIX_MAIN_ENABLE__ // if defined, a main function is included. Used for unit tests
#include "matrix_card.h"

#include <libthecore/utils.h>

#include <cstdint>


const static int MAX_ROWS = 6;
const static int MAX_COLS = 8;
const static int ASLENGTH = 8;

void MatrixCardRndCoordinate(uint32_t & rows, uint32_t & cols)
{
	for (uint32_t i = 0; i < (ASLENGTH >> 1); i++)
	{
		rows |= ((thecore_random() % MAX_ROWS) & 0x000000FF) << ((4 - i - 1) * 8);
		cols |= ((thecore_random() % MAX_COLS) & 0x000000FF) << ((4 - i - 1) * 8);
	}
}

