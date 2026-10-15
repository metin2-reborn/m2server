#pragma once

#include <libthecore/heart.h>

extern heart *thecore_heart;
extern unsigned int thecore_idle_time; // ms spent waiting for pulses; the caller resets it

auto thecore_init(int fps) -> bool;
auto thecore_idle() -> int;
void thecore_shutdown();
void thecore_destroy();
auto thecore_pulse() -> int;
auto thecore_is_shutdowned() -> bool;
void thecore_tick(); // Increase tics
