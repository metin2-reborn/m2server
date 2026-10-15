
#pragma once

#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>

#include <cstdint>

namespace metin2::game::inline empire {

inline constexpr int MAX_CASTLE_GUARD_REGION = 4;
inline constexpr int MAX_CASTLE_GUARD_PER_REGION = 10;
inline constexpr int MAX_CASTLE_FROG = 20;
inline constexpr int MAX_CASTLE_TOWER = 10;
inline constexpr int MIN_CASTLE_TOWER = 5;

inline constexpr int CASTLE_FROG_VNUM = 11505;
constexpr bool is_castle_map(int map) { return map == 181 || map == 182 || map == 183; }

enum castle_state
{
	CASTLE_SIEGE_NONE,
	CASTLE_SIEGE_STRUGGLE,
	CASTLE_SIEGE_END
};

struct castle_data
{
	character *	frog[MAX_CASTLE_FROG];

	character *	guard[MAX_CASTLE_GUARD_REGION][MAX_CASTLE_GUARD_PER_REGION];
	uint32_t		guard_group[MAX_CASTLE_GUARD_REGION][MAX_CASTLE_GUARD_PER_REGION];

	character *	tower[MAX_CASTLE_TOWER];

	event_ptr		siege_event;
	event_ptr		stone_event;
};

bool		castle_boot();
void		castle_save();
int			castle_siege(int empire, int tower_count);
void		castle_start_siege(int empire, int tower_count);
void		castle_end_siege();

character * castle_spawn_frog(int empire);
character *	castle_spawn_guard(int empire, uint32_t group_vnum, int region_index);
bool		castle_spawn_tower(int empire, int tower_count);

void		castle_guard_die(character * ch, character * killer);
void		castle_frog_die(character * ch, character * killer);
void		castle_tower_die(character * ch, character * killer);

int			castle_guard_count(int empire, int region_index);

bool		castle_is_guard_vnum(uint32_t vnum);
int			castle_cost_of_hiring_guard(uint32_t group_vnum);
bool		castle_can_attack(character * ch, character * victim);

bool		castle_is_my_castle(int empire, int map_index);
bool		castle_is_tower_vnum(uint32_t vnum);
}
