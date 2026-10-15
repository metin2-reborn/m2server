/*********************************************************************
 * date        : 2007.11.16
 * file        : ani.h
 * author      : mhh
 * description :
 */

#pragma once

#include "typedef.h"

#include <cstdint>

namespace metin2::game {

void ani_init();
auto ani_attack_speed(CHARACTER *ch) -> uint32_t;
auto ani_combo_speed(CHARACTER *ch, uint8_t combo) -> uint32_t;
}
