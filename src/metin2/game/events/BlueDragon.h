#pragma once

#include <metin2/game/app/typedef.h>

#include <ctime>

namespace metin2::game::inline events {

extern int BlueDragon_StateBattle (CHARACTER *);
extern time_t UseBlueDragonSkill (CHARACTER *, unsigned int);
extern int BlueDragon_Damage (CHARACTER * me, CHARACTER * attacker, int dam);
}
