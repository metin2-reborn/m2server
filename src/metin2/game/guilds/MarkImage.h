#pragma once

#include <lzo/lzo1x.h>

#include <cstdint>
#include <map>
#include <vector>

namespace metin2::game::inline guilds {

typedef uint32_t pixel;

struct guild_mark
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

struct guild_mark_block
{
	enum
	{
		MARK_PER_BLOCK_WIDTH = 4,
		MARK_PER_BLOCK_HEIGHT = 4,

		WIDTH = guild_mark::WIDTH * MARK_PER_BLOCK_WIDTH,
		HEIGHT = guild_mark::HEIGHT * MARK_PER_BLOCK_HEIGHT,

		SIZE = WIDTH * HEIGHT,
		MAX_COMP_SIZE = (SIZE * sizeof(pixel)) + ((SIZE * sizeof(pixel)) >> 4) + 64 + 3 
	};

	///////////////////////////////////////////////////////////////////////////////

	uint8_t 	comp_buf[MAX_COMP_SIZE];	// compressed data
	lzo_uint size_comp_buf;	// compressed size
	uint32_t	crc;			// CRC of the compressed data

	///////////////////////////////////////////////////////////////////////////////

	void	compress(const pixel * pxBuf);
};

class guild_mark_image
{
	public:
		enum
		{
			WIDTH = 512,
			HEIGHT = 512,

			BLOCK_ROW_COUNT = HEIGHT / guild_mark_block::HEIGHT, // 10
			BLOCK_COL_COUNT = WIDTH / guild_mark_block::WIDTH, // 8

			BLOCK_TOTAL_COUNT = BLOCK_ROW_COUNT * BLOCK_COL_COUNT, // 80

			MARK_ROW_COUNT = BLOCK_ROW_COUNT * guild_mark_block::MARK_PER_BLOCK_HEIGHT, // 40
			MARK_COL_COUNT = BLOCK_COL_COUNT * guild_mark_block::MARK_PER_BLOCK_WIDTH, // 32

			MARK_TOTAL_COUNT = MARK_ROW_COUNT * MARK_COL_COUNT, // 1280

		};

		guild_mark_image();

		bool build(const char * c_szFileName);
		bool save(const char* c_szFileName);
		bool load(const char* c_szFileName);

		void put_data(uint32_t x, uint32_t y, uint32_t width, uint32_t height, void* data);
		void data(uint32_t x, uint32_t y, uint32_t width, uint32_t height, void* data);

		bool save_mark(uint32_t posMark, uint8_t * pbMarkImage);
		bool delete_mark(uint32_t posMark);

		void diff_blocks(const uint32_t * crcList, std::map<uint8_t, const guild_mark_block *> & mapDiffBlocks);

	private:
		void	build_all_blocks();

		guild_mark_block	aak_block_[BLOCK_ROW_COUNT][BLOCK_COL_COUNT];
		std::vector<pixel> pixels_;	// WIDTH x HEIGHT, top row first
};
}
