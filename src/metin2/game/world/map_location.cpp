
#include <metin2/game/world/map_location.h>

#include <metin2/game/world/sectree_manager.h>

#include <metin2/log/log.h>

#include <cstdint>
#include <map>
#include <utility>

#include <arpa/inet.h>

namespace metin2::game::inline world {

    auto map_locations::get(int32_t x, int32_t y, int32_t &index, int32_t &addr, uint16_t &port) -> bool {
        index = get_sectree_manager().map_index(x, y);

        return get(index, addr, port);
    }

    auto map_locations::get(int index, int32_t &addr_value, uint16_t &port_value) -> bool {
        if (index == 0) {
            metin2::log::warn("Cannot look up server for invalid map index {:d}", index);
            return false;
        }

        auto it = addresses_.find(index);

        if (addresses_.end() == it) {
            metin2::log::warn("No server known for map {:d}", index);
            std::map<int32_t, location>::iterator i;
            for (i = addresses_.begin(); i != addresses_.end(); ++i) {
                metin2::log::debug("Known map {:d} on server {:x}:{:d}", i->first, i->second.addr, i->second.port);
            }
            return false;
        }

        addr_value = it->second.addr;
        port_value = it->second.port;
        return true;
    }

    void map_locations::insert(int32_t index, const char *host, uint16_t port_value) {
        location loc;

        loc.addr = inet_addr(host);
        loc.port = port_value;

        addresses_.insert(std::make_pair(index, loc));
        metin2::log::info("Map {:d} is on server {}:{:d}", index, host, port_value);
    }
}
