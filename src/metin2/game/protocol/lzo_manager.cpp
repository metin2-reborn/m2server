#include <metin2/game/protocol/lzo_manager.h>

#include <lzo/lzo1x.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace metin2::game::inline protocol {

lzo_manager::lzo_manager()
{
	if (lzo_init() != LZO_E_OK)
	{
		fprintf(stderr, "lzo_init() failed\n");
		abort();
	}

	workmem_ = (uint8_t *) malloc(LZO1X_MEM_COMPRESS);
	memset( workmem_, 0, LZO1X_MEM_COMPRESS );
}

lzo_manager::~lzo_manager()
{
	free(workmem_);
	workmem_ = nullptr;
}

bool lzo_manager::compress(const uint8_t* src, size_t srcsize, uint8_t* dest, lzo_uint * puiDestSize)
{
	int ret = lzo1x_1_compress(src, srcsize, dest, puiDestSize, work_memory());

	if (ret != LZO_E_OK)
		return false;

	return true;
}

bool lzo_manager::decompress(const uint8_t * src, size_t srcsize, uint8_t * dest, lzo_uint * puiDestSize)
{
	int ret = lzo1x_decompress_safe(src, srcsize, dest, puiDestSize, work_memory());

	if (ret != LZO_E_OK)
		return false;

	return true;
}

size_t lzo_manager::max_compressed_size(size_t original)
{
	return (original + (original >> 4) + 64 + 3);
}
}
