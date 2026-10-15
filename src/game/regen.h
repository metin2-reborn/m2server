#pragma once

#include "dungeon.h"
#include "event.h"
#include "typedef.h"

#include <cstddef>
#include <cstdint>

namespace metin2::game {

enum
{
	REGEN_TYPE_MOB,
	REGEN_TYPE_GROUP,
	REGEN_TYPE_EXCEPTION,
	REGEN_TYPE_GROUP_GROUP,
	REGEN_TYPE_ANYWHERE,
	REGEN_TYPE_MAX_NUM
};

struct REGEN {
	REGEN * prev;
	REGEN * next;
	int32_t	lMapIndex;
	int		type;
	int		sx, sy, ex, ey;
	uint8_t	z_section;

	uint8_t	direction;

	uint32_t	time;

	int		max_count;
	int		count;
	int 	vnum;

	bool	is_aggressive;

	LPEVENT	event;

	size_t id; // to help dungeon regen identification

	REGEN() :
		prev(nullptr), next(nullptr),
		lMapIndex(0),
		type(0),
		sx(0), sy(0), ex(0), ey(0),
		z_section(0),
		direction(0),
		time(0),
		max_count(0),
		count(0),
		vnum(0),
		is_aggressive(0),
		event(nullptr),
		id(0)
	{}
};

EVENTINFO(regen_event_info)
{
	REGEN * 	regen;

	regen_event_info() 
	: regen( nullptr )
	{
	}
};


struct REGEN_EXCEPTION {
	REGEN_EXCEPTION * prev;
	REGEN_EXCEPTION * next;

	int		sx, sy, ex, ey;
	uint8_t	z_section;
};

class CDungeon;

EVENTINFO(dungeon_regen_event_info)
{
	REGEN * 	regen;
	CDungeon::IdType dungeon_id;

	dungeon_regen_event_info() 
	: regen( nullptr )
	, dungeon_id( 0 )
	{
	}
};

extern bool	regen_load(const char *filename, int32_t lMapIndex, int base_x, int base_y);
extern bool	regen_do(const char* filename, int32_t lMapIndex, int base_x, int base_y, CDungeon * pDungeon, bool bOnce = true );
extern bool	regen_load_in_file(const char* filename, int32_t lMapIndex, int base_x, int base_y );
extern void	regen_free();

extern void	regen_reset(int x, int y);
}
