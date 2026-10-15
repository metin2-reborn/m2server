#pragma once

#include <chrono>

namespace metin2::inline core {

struct heart {
    std::chrono::steady_clock::duration opt_time;
    std::chrono::steady_clock::time_point last_time;
    int passes_per_sec;
    int pulse;
};
}
