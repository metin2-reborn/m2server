// State and functions shared between libthecore's sources only.
#pragma once

#include <metin2/core/heart.h>

#include <csignal>

namespace metin2::inline core {

// Written from signal handlers.
extern volatile std::sig_atomic_t tics;
extern volatile std::sig_atomic_t shutdowned;

void signal_setup();

auto heart_new(int fps) -> heart *;
auto heart_idle(heart *ht) -> int; // Returns how many pulses have passed.
}
