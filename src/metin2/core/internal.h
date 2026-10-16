#pragma once

#include <metin2/core/heart.h>

#include <atomic>
#include <csignal>

namespace metin2::inline core {

    extern std::atomic<unsigned> ticks;
    extern volatile std::sig_atomic_t shutdowned;

    void signal_setup();

    auto heart_new(int fps) -> heart *;
    auto heart_idle(heart *ht) -> int;
}
