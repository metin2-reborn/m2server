/*********************************************************************
 * date        : 2007.11.16
 * file        : ani.h
 * author      : mhh
 * description :
 */

#pragma once

#include <metin2/game/app/typedef.h>

#include <cstdint>

namespace metin2::game::inline characters {

void ani_init();
auto ani_attack_speed(character *ch) -> uint32_t;
auto ani_combo_speed(character *ch, uint8_t combo) -> uint32_t;
}
