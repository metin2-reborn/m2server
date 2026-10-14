#pragma once

#include "typedef.h"

#include <ctime>

extern int BlueDragon_StateBattle (CHARACTER *);
extern time_t UseBlueDragonSkill (CHARACTER *, unsigned int);
extern int BlueDragon_Damage (CHARACTER * me, CHARACTER * attacker, int dam);

