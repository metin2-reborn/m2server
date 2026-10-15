#include <metin2/core/heart.h>

#include <metin2/core/internal.h>

#include <metin2/log/log.h>

#include <chrono>
#include <thread>

namespace metin2::inline core {

using clock_type = std::chrono::steady_clock;

auto heart_new(int fps) -> heart * {
    auto ht = new heart{};
    ht->opt_time = std::chrono::duration_cast<clock_type::duration>(std::chrono::seconds(1)) / fps;
    ht->passes_per_sec = fps;
    ht->last_time = clock_type::now();
    return ht;
}

auto heart_idle(heart *ht) -> int {
    const clock_type::time_point now = clock_type::now();
    const clock_type::duration process_time = now - ht->last_time;

    int missed_pulse = 0;
    if (process_time >= ht->opt_time) {
        missed_pulse = static_cast<int>(process_time / ht->opt_time);
    }

    if (missed_pulse > 0) {
        ht->last_time = now;
    } else {
        ht->last_time += ht->opt_time;
        std::this_thread::sleep_until(ht->last_time);
    }

    ++missed_pulse;

    if (missed_pulse > (30 * ht->passes_per_sec)) {
        metin2::log::error("losing {:d} seconds. (lag occured)", missed_pulse / ht->passes_per_sec);
        missed_pulse = 30 * ht->passes_per_sec;
    }

    return missed_pulse;
}
}
