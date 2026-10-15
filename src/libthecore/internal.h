// State and functions shared between libthecore's sources only.
#pragma once

#include <libthecore/heart.h>

#include <csignal>

// Written from signal handlers.
extern volatile std::sig_atomic_t tics;
extern volatile std::sig_atomic_t shutdowned;

void signal_setup();

auto log_init() -> bool;
void log_destroy();

auto heart_new(int fps) -> heart *;
auto heart_idle(heart *ht) -> int; // Returns how many pulses have passed.
