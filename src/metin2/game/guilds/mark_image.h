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


	uint8_t 	comp_buf[MAX_COMP_SIZE];
	lzo_uint size_comp_buf;
	uint32_t	crc;


	void	compress(const pixel * px_buf);
};

class guild_mark_image
{
	public:
		enum
		{
			WIDTH = 512,
			HEIGHT = 512,

			BLOCK_ROW_COUNT = HEIGHT / guild_mark_block::HEIGHT,
			BLOCK_COL_COUNT = WIDTH / guild_mark_block::WIDTH,

			BLOCK_TOTAL_COUNT = BLOCK_ROW_COUNT * BLOCK_COL_COUNT,

			MARK_ROW_COUNT = BLOCK_ROW_COUNT * guild_mark_block::MARK_PER_BLOCK_HEIGHT,
			MARK_COL_COUNT = BLOCK_COL_COUNT * guild_mark_block::MARK_PER_BLOCK_WIDTH,

			MARK_TOTAL_COUNT = MARK_ROW_COUNT * MARK_COL_COUNT,

		};

		guild_mark_image();

		bool build(const char * file_name);
		bool save(const char* file_name);
		bool load(const char* file_name);

		void put_data(uint32_t x, uint32_t y, uint32_t width, uint32_t height, void* data);
		void data(uint32_t x, uint32_t y, uint32_t width, uint32_t height, void* data);

		bool save_mark(uint32_t pos_mark, uint8_t * image);
		bool delete_mark(uint32_t pos_mark);

		void diff_blocks(const uint32_t * crc_list, std::map<uint8_t, const guild_mark_block *> & diff_blocks);

	private:
		void	build_all_blocks();

		guild_mark_block	aak_block_[BLOCK_ROW_COUNT][BLOCK_COL_COUNT];
		std::vector<pixel> pixels_;
};
}
