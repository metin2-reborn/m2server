#pragma once
#include <cstdint>
#include <list>
#include <unordered_set>
#include <vector>

namespace metin2::game::inline characters {
    class CHARACTER;
}
namespace metin2::game::inline guilds {
    namespace building {
        class CObject;
    }
}
namespace metin2::game::inline items {
    class CItem;
}
namespace metin2::game::inline parties {
    class CParty;
}
namespace metin2::game::inline protocol {
    class CLIENT_DESC;
    class DESC;
    class DESC_P2P;
}
namespace metin2::game::inline world {
    class CDungeon;
    class CEntity;
    class SECTREE;
    class SECTREE_MAP;
    struct REGEN;
    struct REGEN_EXCEPTION;
}

namespace metin2::game::inline app {




typedef std::vector<CHARACTER *> CHARACTER_VECTOR;
typedef std::list<CHARACTER *> CHARACTER_LIST;
typedef std::unordered_set<CHARACTER *> CHARACTER_SET;



typedef std::unordered_set<CEntity *> ENTITY_SET;

using SECTREE_LIST = std::list<SECTREE *>;





struct PIXEL_POSITION {
	int32_t x, y, z;
};

enum EEntityTypes
{
	ENTITY_CHARACTER,
	ENTITY_ITEM,
	ENTITY_OBJECT,
};
}
