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

		bool    get(int32_t x, int32_t y, int32_t & index, int32_t & addr, uint16_t & port);
		bool	get(int index, int32_t & addr_value, uint16_t & port_value);
		void    insert(int32_t index, const char * host, uint16_t port_value);

	protected:
		std::map<int32_t, location> map_address_;
};      
}
