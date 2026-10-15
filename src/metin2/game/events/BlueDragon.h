#pragma once

#include <metin2/game/app/typedef.h>

#include <ctime>

namespace metin2::game::inline events {

extern int blue_dragon_state_battle (character *);
extern time_t use_blue_dragon_skill (character *, unsigned int);
extern int blue_dragon_damage (character * me, character * attacker, int dam);
}
