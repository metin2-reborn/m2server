#pragma once
#include <cstdint>
#include <list>
#include <unordered_set>
#include <vector>

namespace metin2::game::inline characters {
    class character;
}
namespace metin2::game::inline guilds {
    namespace building {
        class object;
    }
}
namespace metin2::game::inline items {
    class item;
}
namespace metin2::game::inline parties {
    class party;
}
namespace metin2::game::inline protocol {
    class client_desc;
    class desc;
    class desc_p2_p;
}
namespace metin2::game::inline world {
    class dungeon;
    class entity;
    class sectree;
    class sectree_map;
    struct regen;
    struct regen_exception;
}

namespace metin2::game::inline app {




typedef std::vector<character *> character_vector;
typedef std::list<character *> character_list;
typedef std::unordered_set<character *> character_set;



typedef std::unordered_set<entity *> entity_set;

using sectree_list = std::list<sectree *>;





struct pixel_position {
	int32_t x, y, z;
};

enum entity_types
{
	ENTITY_CHARACTER,
	ENTITY_ITEM,
	ENTITY_OBJECT,
};
}
