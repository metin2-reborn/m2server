/*********************************************************************
 * date        : 2007.04.07
 * file        : castle.h
 * author      : mhh
 * description : 
 */

#pragma once

#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>

#include <cstdint>

namespace metin2::game::inline empire {

inline constexpr int MAX_CASTLE_GUARD_REGION = 4; // Guard placement area
inline constexpr int MAX_CASTLE_GUARD_PER_REGION = 10; // Guard groups that can be placed in one area
inline constexpr int MAX_CASTLE_FROG = 20; // Golden toad
inline constexpr int MAX_CASTLE_TOWER = 10; // Max number of beacons summoned
inline constexpr int MIN_CASTLE_TOWER = 5; // Min number of beacons summoned

inline constexpr int CASTLE_FROG_VNUM = 11505; // Golden toad vnum
//#define CASTLE_TOWER_VNUM		11506	// beacon vnum
constexpr bool is_castle_map(int map) { return map == 181 || map == 182 || map == 183; }

enum castle_state
{
	CASTLE_SIEGE_NONE,			// Peace mode
	CASTLE_SIEGE_STRUGGLE,		// Under siege
	CASTLE_SIEGE_END			// If the defense succeeded, beacons can be destroyed for 1 hour.
};

struct castle_data
{
	character *	frog[MAX_CASTLE_FROG];	// Golden toad

	character *	guard[MAX_CASTLE_GUARD_REGION][MAX_CASTLE_GUARD_PER_REGION];	// Guard leader
	uint32_t		guard_group[MAX_CASTLE_GUARD_REGION][MAX_CASTLE_GUARD_PER_REGION];	// Guard leader

	character *	tower[MAX_CASTLE_TOWER];	// Beacon

	event_ptr		siege_event;
	event_ptr		stone_event;
};

/* extern functions */
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
int			castle_cost_of_hiring_guard(uint32_t vnum);
bool		castle_can_attack(character * ch, character * victim);

bool		castle_is_my_castle(int empire, int map_index);
bool		castle_is_tower_vnum(uint32_t vnum);
}
