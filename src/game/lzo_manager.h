#ifndef __INC_LZO_MANAGER_H
#define __INC_LZO_MANAGER_H

#include <common/singleton.h>

#include <lzo/lzo1x.h>

#include <cstddef>
#include <cstdint>

class LZOManager : public singleton<LZOManager>
{
	public:
		LZOManager();
		~LZOManager() override;

		bool	Compress(const uint8_t* src, size_t srcsize, uint8_t* dest, lzo_uint * puiDestSize);
		bool	Decompress(const uint8_t* src, size_t srcsize, uint8_t* dest, lzo_uint * puiDestSize);
		size_t	GetMaxCompressedSize(size_t original);

		uint8_t *	GetWorkMemory() { return m_workmem; }

	private:
		uint8_t *	m_workmem;
};

#endif
