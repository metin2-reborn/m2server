#pragma once
#include <cstdint>
#include <list>
#include <unordered_set>
#include <vector>

class DESC;

class CLIENT_DESC;

class DESC_P2P;

class CHARACTER;
typedef std::vector<CHARACTER *> CHARACTER_VECTOR;
typedef std::list<CHARACTER *> CHARACTER_LIST;
typedef std::unordered_set<CHARACTER *> CHARACTER_SET;

class CItem;

namespace building {
class CObject;
}

class CEntity;
typedef std::unordered_set<CEntity *> ENTITY_SET;

class SECTREE;
using SECTREE_LIST = std::list<SECTREE *>;

class SECTREE_MAP;

class CDungeon;

class CParty;

struct SRegen;
struct SRegenException;
using REGEN = SRegen;
using REGEN_EXCEPTION = SRegenException;

typedef struct pixel_position_s
{
	int32_t x, y, z;
} PIXEL_POSITION;

enum EEntityTypes
{
	ENTITY_CHARACTER,
	ENTITY_ITEM,
	ENTITY_OBJECT,
};
