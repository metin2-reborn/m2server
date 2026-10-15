
#include <metin2/game/world/map_location.h>

#include <metin2/game/world/sectree_manager.h>

#include <metin2/log/log.h>

#include <cstdint>
#include <map>
#include <utility>

#include <arpa/inet.h>

namespace metin2::game::inline world {

map_locations locations;

bool map_locations::get(int32_t x, int32_t y, int32_t & lIndex, int32_t & lAddr, uint16_t & wPort)
{
	lIndex = sectree_manager::instance().map_index(x, y);

	return get(lIndex, lAddr, wPort);
}

bool map_locations::get(int iIndex, int32_t & lAddr, uint16_t & wPort)
{
	if (iIndex == 0)
	{
		metin2::log::info( "CMapLocation::Get - Error MapIndex[{:d}]", iIndex);
		return false;
	}

	std::map<int32_t, location>::iterator it = map_address_.find(iIndex);

	if (map_address_.end() == it)
	{
		metin2::log::info( "CMapLocation::Get - Error MapIndex[{:d}]", iIndex);
		std::map<int32_t, location>::iterator i;
		for ( i	= map_address_.begin(); i != map_address_.end(); ++i)
		{
			metin2::log::info( "Map({:d}): Server({:x}:{:d})", i->first, i->second.addr, i->second.port);
		}
		return false;
	}

	lAddr = it->second.addr;
	wPort = it->second.port;
	return true;
}

void map_locations::insert(int32_t lIndex, const char * c_pszHost, uint16_t wPort)
{
	location loc;

	loc.addr = inet_addr(c_pszHost);
	loc.port = wPort;

	map_address_.insert(std::make_pair(lIndex, loc));
	metin2::log::info( "MapLocation::Insert : {:d} {} {:d}", lIndex, c_pszHost, wPort);
}
}
