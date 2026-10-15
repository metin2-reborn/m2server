
#include <metin2/game/world/map_location.h>

#include <metin2/game/world/sectree_manager.h>

#include <metin2/log/log.h>

#include <cstdint>
#include <map>
#include <utility>

#include <arpa/inet.h>

namespace metin2::game::inline world {

map_locations locations;

bool map_locations::get(int32_t x, int32_t y, int32_t & index, int32_t & addr, uint16_t & port)
{
	index = sectree_manager::instance().map_index(x, y);

	return get(index, addr, port);
}

bool map_locations::get(int index, int32_t & addr_value, uint16_t & port_value)
{
	if (index == 0)
	{
		metin2::log::info( "CMapLocation::Get - Error MapIndex[{:d}]", index);
		return false;
	}

	std::map<int32_t, location>::iterator it = map_address_.find(index);

	if (map_address_.end() == it)
	{
		metin2::log::info( "CMapLocation::Get - Error MapIndex[{:d}]", index);
		std::map<int32_t, location>::iterator i;
		for ( i	= map_address_.begin(); i != map_address_.end(); ++i)
		{
			metin2::log::info( "Map({:d}): Server({:x}:{:d})", i->first, i->second.addr, i->second.port);
		}
		return false;
	}

	addr_value = it->second.addr;
	port_value = it->second.port;
	return true;
}

void map_locations::insert(int32_t index, const char * host, uint16_t port_value)
{
	location loc;

	loc.addr = inet_addr(host);
	loc.port = port_value;

	map_address_.insert(std::make_pair(index, loc));
	metin2::log::info( "MapLocation::Insert : {:d} {} {:d}", index, host, port_value);
}
}
