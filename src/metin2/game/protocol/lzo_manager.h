#pragma once

#include <metin2/common/singleton.h>

#include <lzo/lzo1x.h>

#include <cstddef>
#include <cstdint>

namespace metin2::game::inline protocol {

class lzo_manager : public singleton<lzo_manager>
{
	public:
		lzo_manager();
		~lzo_manager() override;

		bool	compress(const uint8_t* src, size_t srcsize, uint8_t* dest, lzo_uint * pui_dest_size);
		bool	decompress(const uint8_t* src, size_t srcsize, uint8_t* dest, lzo_uint * pui_dest_size);
		size_t	max_compressed_size(size_t original);

		uint8_t *	work_memory() { return workmem_; }

	private:
		uint8_t *	workmem_;
};
}
