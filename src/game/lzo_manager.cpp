#include "lzo_manager.h"

#include <lzo/lzo1x.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

LZOManager::LZOManager()
{
	if (lzo_init() != LZO_E_OK)
	{
		fprintf(stderr, "lzo_init() failed\n");
		abort();
	}

	m_workmem = (uint8_t *) malloc(LZO1X_MEM_COMPRESS);
	memset( m_workmem, 0, LZO1X_MEM_COMPRESS );
}

LZOManager::~LZOManager()
{
	free(m_workmem);
	m_workmem = nullptr;
}

bool LZOManager::Compress(const uint8_t* src, size_t srcsize, uint8_t* dest, lzo_uint * puiDestSize)
{
	int ret = lzo1x_1_compress(src, srcsize, dest, puiDestSize, GetWorkMemory());

	if (ret != LZO_E_OK)
		return false;

	return true;
}

bool LZOManager::Decompress(const uint8_t * src, size_t srcsize, uint8_t * dest, lzo_uint * puiDestSize)
{
	int ret = lzo1x_decompress_safe(src, srcsize, dest, puiDestSize, GetWorkMemory());

	if (ret != LZO_E_OK)
		return false;

	return true;
}

size_t LZOManager::GetMaxCompressedSize(size_t original)
{
	return (original + (original >> 4) + 64 + 3);
}

