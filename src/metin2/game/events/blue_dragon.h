#pragma once

#include <metin2/game/app/typedef.h>

#include <ctime>

namespace metin2::game::inline events {

    extern auto blue_dragon_state_battle(character *) -> int;
    extern auto use_blue_dragon_skill(character *, unsigned int) -> time_t;
    extern auto blue_dragon_damage(character *me, character *attacker, int dam) -> int;
}
