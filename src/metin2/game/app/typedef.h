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
    class desc_p2p;
}
namespace metin2::game::inline world {
    class dungeon;
    class entity;
    class sectree;
    class sectree_map;
    struct regen;
}

namespace metin2::game::inline app {

    using character_vector = std::vector<character *>;
    using character_list = std::list<character *>;
    using character_set = std::unordered_set<character *>;

    using entity_set = std::unordered_set<entity *>;

    using sectree_list = std::list<sectree *>;

    struct pixel_position {
        int32_t x, y, z;
    };

    enum entity_types {
        ENTITY_CHARACTER,
        ENTITY_ITEM,
        ENTITY_OBJECT,
    };
}
