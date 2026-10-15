#pragma once

#include <lzo/lzo1x.h>

#include <cstdint>
#include <map>
#include <vector>

namespace metin2::game {

typedef uint32_t Pixel;

struct SGuildMark
{
	enum
	{
		WIDTH = 16,
		HEIGHT = 12,
		SIZE = WIDTH * HEIGHT,
	};

	///////////////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////////////
};

struct SGuildMarkBlock
{
	enum
	{
		MARK_PER_BLOCK_WIDTH = 4,
		MARK_PER_BLOCK_HEIGHT = 4,

		WIDTH = SGuildMark::WIDTH * MARK_PER_BLOCK_WIDTH,
		HEIGHT = SGuildMark::HEIGHT * MARK_PER_BLOCK_HEIGHT,

		SIZE = WIDTH * HEIGHT,
		MAX_COMP_SIZE = (SIZE * sizeof(Pixel)) + ((SIZE * sizeof(Pixel)) >> 4) + 64 + 3 
	};

	///////////////////////////////////////////////////////////////////////////////

	uint8_t 	m_abCompBuf[MAX_COMP_SIZE];	// compressed data
	lzo_uint m_sizeCompBuf;	// compressed size
	uint32_t	m_crc;			// CRC of the compressed data

	///////////////////////////////////////////////////////////////////////////////

	void	Compress(const Pixel * pxBuf);
};

class CGuildMarkImage
{
	public:
		enum
		{
			WIDTH = 512,
			HEIGHT = 512,

			BLOCK_ROW_COUNT = HEIGHT / SGuildMarkBlock::HEIGHT, // 10
			BLOCK_COL_COUNT = WIDTH / SGuildMarkBlock::WIDTH, // 8

			BLOCK_TOTAL_COUNT = BLOCK_ROW_COUNT * BLOCK_COL_COUNT, // 80

			MARK_ROW_COUNT = BLOCK_ROW_COUNT * SGuildMarkBlock::MARK_PER_BLOCK_HEIGHT, // 40
			MARK_COL_COUNT = BLOCK_COL_COUNT * SGuildMarkBlock::MARK_PER_BLOCK_WIDTH, // 32

			MARK_TOTAL_COUNT = MARK_ROW_COUNT * MARK_COL_COUNT, // 1280

		};

		CGuildMarkImage();

		bool Build(const char * c_szFileName);
		bool Save(const char* c_szFileName);
		bool Load(const char* c_szFileName);

		void PutData(uint32_t x, uint32_t y, uint32_t width, uint32_t height, void* data);
		void GetData(uint32_t x, uint32_t y, uint32_t width, uint32_t height, void* data);

		bool SaveMark(uint32_t posMark, uint8_t * pbMarkImage);
		bool DeleteMark(uint32_t posMark);

		void GetDiffBlocks(const uint32_t * crcList, std::map<uint8_t, const SGuildMarkBlock *> & mapDiffBlocks);

	private:
		void	BuildAllBlocks();

		SGuildMarkBlock	m_aakBlock[BLOCK_ROW_COUNT][BLOCK_COL_COUNT];
		std::vector<Pixel> m_pixels;	// WIDTH x HEIGHT, top row first
};
}
