#pragma once

#include <metin2/common/singleton.h>

#include <cstdint>
#include <map>

namespace metin2::game::inline world {

class map_locations : public singleton<map_locations>
{
	public:
		struct location {
			int32_t        addr;
			uint16_t        port;
		};    

		bool    get(int32_t x, int32_t y, int32_t & lMapIndex, int32_t & lAddr, uint16_t & wPort);
		bool	get(int iIndex, int32_t & lAddr, uint16_t & wPort);
		void    insert(int32_t lIndex, const char * c_pszHost, uint16_t wPort);

	protected:
		std::map<int32_t, location> map_address_;
};      
}
