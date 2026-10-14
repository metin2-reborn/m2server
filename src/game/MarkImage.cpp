#include "MarkImage.h"

#include "crc32.h"
#include "image_file.h"
#include "lzo_manager.h"

#include <libthecore/log.h>

#include <lzo/lzo1x.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <utility>
#include <vector>

CGuildMarkImage::CGuildMarkImage() : m_pixels(WIDTH * HEIGHT)
{
}

bool CGuildMarkImage::Save(const char* c_szFileName) 
{
	return save_tga_bgra(c_szFileName, m_pixels.data(), WIDTH, HEIGHT);
}

bool CGuildMarkImage::Build(const char * c_szFileName)
{
	sys_log(0, "GuildMarkImage: creating new file %s", c_szFileName);

	std::ranges::fill(m_pixels, 0);
	return Save(c_szFileName);
}

bool CGuildMarkImage::Load(const char * c_szFileName) 
{
	int width = 0;
	int height = 0;
	std::vector<Pixel> pixels = load_image_bgra(c_szFileName, width, height);

	if (pixels.empty())
	{
		sys_err("GuildMarkImage: %s cannot open file.", c_szFileName);
		return false;
	}

	if (width != WIDTH)
	{
		sys_err("GuildMarkImage: %s width must be %u", c_szFileName, WIDTH);
		return false;
	}

	if (height != HEIGHT)
	{
		sys_err("GuildMarkImage: %s height must be %u", c_szFileName, HEIGHT);
		return false;
	}

	m_pixels = std::move(pixels);
	BuildAllBlocks();
	return true;
}

// Copies a width x height rectangle at (x, y) in or out; the part outside the image is skipped.
void CGuildMarkImage::PutData(uint32_t x, uint32_t y, uint32_t width, uint32_t height, void * data)
{
	const auto * src = static_cast<const Pixel *>(data);
	for (uint32_t row = 0; row < height && y + row < HEIGHT; ++row)
		std::copy_n(src + row * width, std::min<uint32_t>(width, WIDTH - std::min<uint32_t>(x, WIDTH)), m_pixels.begin() + (y + row) * WIDTH + x);
}

void CGuildMarkImage::GetData(uint32_t x, uint32_t y, uint32_t width, uint32_t height, void * data)
{
	auto * dst = static_cast<Pixel *>(data);
	for (uint32_t row = 0; row < height && y + row < HEIGHT; ++row)
		std::copy_n(m_pixels.begin() + (y + row) * WIDTH + x, std::min<uint32_t>(width, WIDTH - std::min<uint32_t>(x, WIDTH)), dst + row * width);
}

// image = 512x512
//   block = 4 x 4 marks
//   mark = 16 x 12
// blocks per image = 8 x 10

// SERVER
bool CGuildMarkImage::SaveMark(uint32_t posMark, uint8_t * pbImage)
{
	if (posMark >= MARK_TOTAL_COUNT)
	{
		sys_err("GuildMarkImage::CopyMarkFromData: Invalid mark position %u", posMark);
		return false;
	}

	// Draw the mark onto the whole image.
	uint32_t colMark = posMark % MARK_COL_COUNT;
	uint32_t rowMark = posMark / MARK_COL_COUNT;

	printf("PutMark pos %u %ux%u\n", posMark, colMark * SGuildMark::WIDTH, rowMark * SGuildMark::HEIGHT);
	PutData(colMark * SGuildMark::WIDTH, rowMark * SGuildMark::HEIGHT, SGuildMark::WIDTH, SGuildMark::HEIGHT, pbImage);

	// Update the blocks where it was drawn
	uint32_t rowBlock = rowMark / SGuildMarkBlock::MARK_PER_BLOCK_HEIGHT;
	uint32_t colBlock = colMark / SGuildMarkBlock::MARK_PER_BLOCK_WIDTH;

	Pixel apxBuf[SGuildMarkBlock::SIZE];
	GetData(colBlock * SGuildMarkBlock::WIDTH, rowBlock * SGuildMarkBlock::HEIGHT, SGuildMarkBlock::WIDTH, SGuildMarkBlock::HEIGHT, apxBuf);
	m_aakBlock[rowBlock][colBlock].Compress(apxBuf);
	return true;
}

bool CGuildMarkImage::DeleteMark(uint32_t posMark)
{
	Pixel image[SGuildMark::SIZE];
	memset(&image, 0, sizeof(image));
	return SaveMark(posMark, (uint8_t *) &image);
}

void CGuildMarkImage::BuildAllBlocks() // Split the whole image into blocks
{
	Pixel apxBuf[SGuildMarkBlock::SIZE];
	sys_log(0, "GuildMarkImage::BuildAllBlocks");

	for (uint32_t row = 0; row < BLOCK_ROW_COUNT; ++row)
		for (uint32_t col = 0; col < BLOCK_COL_COUNT; ++col)
		{
			GetData(col * SGuildMarkBlock::WIDTH, row * SGuildMarkBlock::HEIGHT, SGuildMarkBlock::WIDTH, SGuildMarkBlock::HEIGHT, apxBuf);
			m_aakBlock[row][col].Compress(apxBuf);
		}
}

void CGuildMarkImage::GetDiffBlocks(const uint32_t * crcList, std::map<uint8_t, const SGuildMarkBlock *> & mapDiffBlocks)
{
	uint8_t posBlock = 0;

	for (uint32_t row = 0; row < BLOCK_ROW_COUNT; ++row)
		for (uint32_t col = 0; col < BLOCK_COL_COUNT; ++col)
		{
			if (m_aakBlock[row][col].m_crc != *crcList)
			{
				mapDiffBlocks.insert(std::map<uint8_t, const SGuildMarkBlock *>::value_type(posBlock, &m_aakBlock[row][col]));
			}
			++crcList;
			++posBlock;
		}
}

void SGuildMarkBlock::Compress(const Pixel * pxBuf)
{
	m_sizeCompBuf = MAX_COMP_SIZE;

	if (LZO_E_OK != lzo1x_1_compress((const uint8_t *) pxBuf, sizeof(Pixel) * SGuildMarkBlock::SIZE, m_abCompBuf, &m_sizeCompBuf, LZOManager::instance().GetWorkMemory()))
	{
		sys_err("SGuildMarkBlock::Compress: Error! %zu > %lu", sizeof(Pixel) * SGuildMarkBlock::SIZE, static_cast<unsigned long>(m_sizeCompBuf));
		return;
	}

	m_crc = GetCRC32((const char *) pxBuf, sizeof(Pixel) * SGuildMarkBlock::SIZE);
}
