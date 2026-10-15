#pragma once

#include <metin2/game/world/dungeon.h>
#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>

#include <cstddef>
#include <cstdint>

namespace metin2::game::inline world {

enum
{
	REGEN_TYPE_MOB,
	REGEN_TYPE_GROUP,
	REGEN_TYPE_EXCEPTION,
	REGEN_TYPE_GROUP_GROUP,
	REGEN_TYPE_ANYWHERE,
	REGEN_TYPE_MAX_NUM
};

struct regen {
	regen * prev;
	regen * next;
	int32_t	map_index;
	int		type;
	int		sx, sy, ex, ey;
	uint8_t	z_section;

	uint8_t	direction;

	uint32_t	time;

	int		max_count;
	int		count;
	int 	vnum;

	bool	is_aggressive;

	event_ptr	event;

	size_t id;

	regen() :
		prev(nullptr), next(nullptr),
		map_index(0),
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
	game::world::regen * 	regen;

	regen_event_info() 
	: regen( nullptr )
	{
	}
};


struct regen_exception {
	regen_exception * prev;
	regen_exception * next;

	int		sx, sy, ex, ey;
	uint8_t	z_section;
};

class dungeon;

EVENTINFO(dungeon_regen_event_info)
{
	game::world::regen * 	regen;
	dungeon::id_type dungeon_id;

	dungeon_regen_event_info() 
	: regen( nullptr )
	, dungeon_id( 0 )
	{
	}
};

extern auto	regen_load(const char *filename, int32_t map_index_value, int base_x, int base_y) -> bool;
extern auto	regen_do(const char* filename, int32_t map_index_value, int base_x, int base_y, dungeon * dungeon_value, bool once = true ) -> bool;
extern auto	regen_load_in_file(const char* filename, int32_t map_index_value, int base_x, int base_y ) -> bool;
extern void	regen_free();

extern void	regen_reset(int x, int y);
}
