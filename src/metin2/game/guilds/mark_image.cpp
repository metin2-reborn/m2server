#include <metin2/game/guilds/mark_image.h>

#include <metin2/game/app/crc32.h>
#include <metin2/game/data/image_file.h>
#include <metin2/game/protocol/lzo_manager.h>

#include <metin2/log/log.h>

#include <lzo/lzo1x.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <utility>
#include <vector>

namespace metin2::game::inline guilds {

guild_mark_image::guild_mark_image() : pixels_(WIDTH * HEIGHT)
{
}

bool guild_mark_image::save(const char* c_szFileName) 
{
	return save_tga_bgra(c_szFileName, pixels_.data(), WIDTH, HEIGHT);
}

bool guild_mark_image::build(const char * c_szFileName)
{
	metin2::log::info( "GuildMarkImage: creating new file {}", c_szFileName);

	std::ranges::fill(pixels_, 0);
	return save(c_szFileName);
}

bool guild_mark_image::load(const char * c_szFileName) 
{
	int width = 0;
	int height = 0;
	std::vector<pixel> pixels = load_image_bgra(c_szFileName, width, height);

	if (pixels.empty())
	{
		metin2::log::error("GuildMarkImage: {} cannot open file.", c_szFileName);
		return false;
	}

	if (width != WIDTH)
	{
		metin2::log::error("GuildMarkImage: {} width must be {:d}", c_szFileName, WIDTH);
		return false;
	}

	if (height != HEIGHT)
	{
		metin2::log::error("GuildMarkImage: {} height must be {:d}", c_szFileName, HEIGHT);
		return false;
	}

	pixels_ = std::move(pixels);
	build_all_blocks();
	return true;
}

// Copies a width x height rectangle at (x, y) in or out; the part outside the image is skipped.
void guild_mark_image::put_data(uint32_t x, uint32_t y, uint32_t width, uint32_t height, void * data)
{
	const auto * src = static_cast<const pixel *>(data);
	for (uint32_t row = 0; row < height && y + row < HEIGHT; ++row)
		std::copy_n(src + row * width, std::min<uint32_t>(width, WIDTH - std::min<uint32_t>(x, WIDTH)), pixels_.begin() + (y + row) * WIDTH + x);
}

void guild_mark_image::data(uint32_t x, uint32_t y, uint32_t width, uint32_t height, void * data)
{
	auto * dst = static_cast<pixel *>(data);
	for (uint32_t row = 0; row < height && y + row < HEIGHT; ++row)
		std::copy_n(pixels_.begin() + (y + row) * WIDTH + x, std::min<uint32_t>(width, WIDTH - std::min<uint32_t>(x, WIDTH)), dst + row * width);
}

// image = 512x512
//   block = 4 x 4 marks
//   mark = 16 x 12
// blocks per image = 8 x 10

// SERVER
bool guild_mark_image::save_mark(uint32_t posMark, uint8_t * pbImage)
{
	if (posMark >= MARK_TOTAL_COUNT)
	{
		metin2::log::error("GuildMarkImage::CopyMarkFromData: Invalid mark position {:d}", posMark);
		return false;
	}

	// Draw the mark onto the whole image.
	uint32_t colMark = posMark % MARK_COL_COUNT;
	uint32_t rowMark = posMark / MARK_COL_COUNT;

	printf("PutMark pos %u %ux%u\n", posMark, colMark * guild_mark::WIDTH, rowMark * guild_mark::HEIGHT);
	put_data(colMark * guild_mark::WIDTH, rowMark * guild_mark::HEIGHT, guild_mark::WIDTH, guild_mark::HEIGHT, pbImage);

	// Update the blocks where it was drawn
	uint32_t rowBlock = rowMark / guild_mark_block::MARK_PER_BLOCK_HEIGHT;
	uint32_t colBlock = colMark / guild_mark_block::MARK_PER_BLOCK_WIDTH;

	pixel apxBuf[guild_mark_block::SIZE];
	data(colBlock * guild_mark_block::WIDTH, rowBlock * guild_mark_block::HEIGHT, guild_mark_block::WIDTH, guild_mark_block::HEIGHT, apxBuf);
	aak_block_[rowBlock][colBlock].compress(apxBuf);
	return true;
}

bool guild_mark_image::delete_mark(uint32_t posMark)
{
	pixel image[guild_mark::SIZE];
	memset(&image, 0, sizeof(image));
	return save_mark(posMark, (uint8_t *) &image);
}

void guild_mark_image::build_all_blocks() // Split the whole image into blocks
{
	pixel apxBuf[guild_mark_block::SIZE];
	metin2::log::info( "GuildMarkImage::BuildAllBlocks");

	for (uint32_t row = 0; row < BLOCK_ROW_COUNT; ++row)
		for (uint32_t col = 0; col < BLOCK_COL_COUNT; ++col)
		{
			data(col * guild_mark_block::WIDTH, row * guild_mark_block::HEIGHT, guild_mark_block::WIDTH, guild_mark_block::HEIGHT, apxBuf);
			aak_block_[row][col].compress(apxBuf);
		}
}

void guild_mark_image::diff_blocks(const uint32_t * crcList, std::map<uint8_t, const guild_mark_block *> & mapDiffBlocks)
{
	uint8_t posBlock = 0;

	for (uint32_t row = 0; row < BLOCK_ROW_COUNT; ++row)
		for (uint32_t col = 0; col < BLOCK_COL_COUNT; ++col)
		{
			if (aak_block_[row][col].crc != *crcList)
			{
				mapDiffBlocks.insert(std::map<uint8_t, const guild_mark_block *>::value_type(posBlock, &aak_block_[row][col]));
			}
			++crcList;
			++posBlock;
		}
}

void guild_mark_block::compress(const pixel * pxBuf)
{
	size_comp_buf = MAX_COMP_SIZE;

	if (LZO_E_OK != lzo1x_1_compress((const uint8_t *) pxBuf, sizeof(pixel) * guild_mark_block::SIZE, comp_buf, &size_comp_buf, lzo_manager::instance().work_memory()))
	{
		metin2::log::error("SGuildMarkBlock::Compress: Error! {:d} > {:d}", sizeof(pixel) * guild_mark_block::SIZE, static_cast<unsigned long>(size_comp_buf));
		return;
	}

	crc = get_cr_c32((const char *) pxBuf, sizeof(pixel) * guild_mark_block::SIZE);
}
}
