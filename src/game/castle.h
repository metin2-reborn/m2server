/*********************************************************************
 * date        : 2007.04.07
 * file        : castle.h
 * author      : mhh
 * description : 
 */

#ifndef _castle_h_
#define _castle_h_


#include "event.h"
#include "typedef.h"

#include <cstdint>

inline constexpr int MAX_CASTLE_GUARD_REGION = 4; // Guard placement area
inline constexpr int MAX_CASTLE_GUARD_PER_REGION = 10; // Guard groups that can be placed in one area
inline constexpr int MAX_CASTLE_FROG = 20; // Golden toad
inline constexpr int MAX_CASTLE_TOWER = 10; // Max number of beacons summoned
inline constexpr int MIN_CASTLE_TOWER = 5; // Min number of beacons summoned


inline constexpr int CASTLE_FROG_VNUM = 11505; // Golden toad vnum
//#define CASTLE_TOWER_VNUM		11506	// beacon vnum
constexpr bool IS_CASTLE_MAP(int map) { return map == 181 || map == 182 || map == 183; }


enum CASTLE_STATE
{
	CASTLE_SIEGE_NONE,			// Peace mode
	CASTLE_SIEGE_STRUGGLE,		// Under siege
	CASTLE_SIEGE_END			// If the defense succeeded, beacons can be destroyed for 1 hour.
};


struct CASTLE_DATA
{
	CHARACTER *	frog[MAX_CASTLE_FROG];	// Golden toad

	CHARACTER *	guard[MAX_CASTLE_GUARD_REGION][MAX_CASTLE_GUARD_PER_REGION];	// Guard leader
	uint32_t		guard_group[MAX_CASTLE_GUARD_REGION][MAX_CASTLE_GUARD_PER_REGION];	// Guard leader

	CHARACTER *	tower[MAX_CASTLE_TOWER];	// Beacon

	LPEVENT		siege_event;
	LPEVENT		stone_event;
};




/* extern functions */
bool		castle_boot();
void		castle_save();
int			castle_siege(int empire, int tower_count);
void		castle_start_siege(int empire, int tower_count);
void		castle_end_siege();

CHARACTER * castle_spawn_frog(int empire);
CHARACTER *	castle_spawn_guard(int empire, uint32_t group_vnum, int region_index);
bool		castle_spawn_tower(int empire, int tower_count);

void		castle_guard_die(CHARACTER * ch, CHARACTER * killer);
void		castle_frog_die(CHARACTER * ch, CHARACTER * killer);
void		castle_tower_die(CHARACTER * ch, CHARACTER * killer);

int			castle_guard_count(int empire, int region_index);

bool		castle_is_guard_vnum(uint32_t vnum);
int			castle_cost_of_hiring_guard(uint32_t vnum);
bool		castle_can_attack(CHARACTER * ch, CHARACTER * victim);

bool		castle_is_my_castle(int empire, int map_index);
bool		castle_is_tower_vnum(uint32_t vnum);

#endif	/* _castle_h_ */

